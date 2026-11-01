#include <pch.h>

#include <Zeyrixon/Physics/PhysicsWorld.h>
#include <Zeyrixon/Physics/Collider.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Core/Log.h>

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace Zeyrixon
{
    namespace
    {
        // Bullet and glm both describe the same things with different types; these are the only conversions.
        btVector3 ToBullet(const glm::vec3& v) { return btVector3(v.x, v.y, v.z); }
        btQuaternion ToBullet(const glm::quat& q) { return btQuaternion(q.x, q.y, q.z, q.w); }
        glm::vec3 ToGlm(const btVector3& v) { return glm::vec3(v.x(), v.y(), v.z()); }
        glm::quat ToGlm(const btQuaternion& q) { return glm::quat(q.w(), q.x(), q.y(), q.z()); }   // glm's constructor is (w, x, y, z)

        /* What a swept shape is allowed to hit. A sweep reports the closest thing in its way; this callback
           decides what counts as "in its way". Only the mover's own body and non-static things are excluded:
           walls and floors block it, crates and other movers do not. */
        struct SolidSweepCallback : btCollisionWorld::ClosestConvexResultCallback
        {
            const btCollisionObject* Self;

            SolidSweepCallback(const btVector3& from, const btVector3& to, const btCollisionObject* self)
                : ClosestConvexResultCallback(from, to), Self(self)
            {
                // Static bodies live in the StaticFilter group. (Kinematic ones do too, which is why
                // needsCollision below still has to rule them out by hand.)
                m_collisionFilterGroup = btBroadphaseProxy::DefaultFilter;
                m_collisionFilterMask = btBroadphaseProxy::StaticFilter;
            }

            bool needsCollision(btBroadphaseProxy* proxy) const override
            {
                const auto* object = static_cast<const btCollisionObject*>(proxy->m_clientObject);
                if (object == Self || object->isKinematicObject())
                    return false;
                return ClosestConvexResultCallback::needsCollision(proxy);
            }
        };

        std::unique_ptr<btCollisionShape> MakeShape(ColliderShape type, const Physics::ColliderShapeWorld& s)
        {
            switch (type)
            {
                case ColliderShape::Sphere:
                    return std::make_unique<btSphereShape>(s.Radius);

                case ColliderShape::Capsule:
                {
                    // Bullet's capsule wants the length of the straight part only, not including the caps.
                    const float cylinderHeight = std::max(2.0f * (s.HalfHeight - s.Radius), 0.0f);
                    return std::make_unique<btCapsuleShape>(s.Radius, cylinderHeight);   // axis = Y
                }

                default:
                    return std::make_unique<btBoxShape>(ToBullet(s.HalfExtents));
            }
        }
    }

    struct PhysicsWorld::Impl
    {
        /* One simulated entity. The members are declared in the order they must be DESTROYED in, reversed:
           the body goes first, then the motion state and the shape it points to. */
        struct Body
        {
            std::unique_ptr<btCollisionShape> Shape;
            btConvexShape* Convex = nullptr;   // the same object as Shape; the sweep test needs the convex interface
            std::unique_ptr<btDefaultMotionState> MotionState;
            std::unique_ptr<btRigidBody> RigidBody;

            bool Dynamic = false;   // moved by the simulation (as opposed to Static / Kinematic)

            // The Transform values we last agreed on with Bullet. If the entity's transform differs from
            // these, somebody else (a script) moved it, and the body has to follow.
            glm::vec3 LastPosition{ 0.0f };
            glm::vec3 LastRotation{ 0.0f };
        };

        // Order matters: Bullet's world keeps pointers to the other four, so they must outlive it.
        btDefaultCollisionConfiguration Configuration;
        btCollisionDispatcher Dispatcher{ &Configuration };
        btDbvtBroadphase Broadphase;
        btSequentialImpulseConstraintSolver Solver;
        btDiscreteDynamicsWorld World{ &Dispatcher, &Broadphase, &Solver, &Configuration };

        std::unordered_map<entt::entity, Body> Bodies;

        // Bodies must be taken OUT of the world before they are destroyed. btDiscreteDynamicsWorld's own
        // destructor walks the bodies still inside it, so freeing one first is a use-after-free.
        void Clear()
        {
            for (auto& [entity, body] : Bodies)
                World.removeRigidBody(body.RigidBody.get());
            Bodies.clear();
        }

        void CreateBody(entt::registry& registry, entt::entity entity,
                        const TransformComponent& transform, const ColliderComponent& collider)
        {
            const Physics::ColliderShapeWorld shape = Physics::ComputeWorldShape(collider, transform);

            Body body;
            body.Shape = MakeShape(collider.Shape, shape);
            body.Convex = static_cast<btConvexShape*>(body.Shape.get());   // box, sphere and capsule are all convex

            // A collider with no RigidBody is simply a static obstacle (floors, walls).
            const RigidBodyComponent* settings = registry.all_of<RigidBodyComponent>(entity)
                ? &registry.get<RigidBodyComponent>(entity) : nullptr;

            const BodyType type = settings ? settings->Type : BodyType::Static;
            body.Dynamic = (type == BodyType::Dynamic);

            // Mass 0 is how Bullet says "immovable"; only dynamic bodies get a real mass.
            const float mass = body.Dynamic ? std::max(settings->Mass, 0.0001f) : 0.0f;

            btVector3 inertia(0.0f, 0.0f, 0.0f);
            if (mass > 0.0f)
                body.Shape->calculateLocalInertia(mass, inertia);   // how hard it is to spin, from its shape

            btTransform start;
            start.setIdentity();
            start.setOrigin(ToBullet(shape.Center));
            start.setRotation(ToBullet(shape.Rotation));
            body.MotionState = std::make_unique<btDefaultMotionState>(start);

            btRigidBody::btRigidBodyConstructionInfo info(mass, body.MotionState.get(), body.Shape.get(), inertia);
            if (settings)
            {
                info.m_friction = settings->Friction;
                info.m_restitution = settings->Restitution;
                info.m_linearDamping = settings->LinearDamping;
                info.m_angularDamping = settings->AngularDamping;
            }
            body.RigidBody = std::make_unique<btRigidBody>(info);

            if (type == BodyType::Kinematic)
            {
                // Moved by us, but still pushes dynamic bodies around. It must never fall asleep, or it
                // would stop pushing.
                body.RigidBody->setCollisionFlags(body.RigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
                body.RigidBody->setActivationState(DISABLE_DEACTIVATION);
            }

            if (body.Dynamic && settings)
            {
                if (settings->FixedRotation)
                    body.RigidBody->setAngularFactor(btVector3(0.0f, 0.0f, 0.0f));   // can't turn on any axis

                if (!settings->UseGravity)
                {
                    // addRigidBody gives every body the world's gravity UNLESS this flag is set, so the
                    // flag has to be in place before the body is added.
                    body.RigidBody->setFlags(body.RigidBody->getFlags() | BT_DISABLE_WORLD_GRAVITY);
                    body.RigidBody->setGravity(btVector3(0.0f, 0.0f, 0.0f));
                }
            }

            body.LastPosition = transform.Position;
            body.LastRotation = transform.Rotation;

            World.addRigidBody(body.RigidBody.get());
            Bodies.emplace(entity, std::move(body));
        }

        // Teleports a body to where its entity's transform says it should be.
        void PushTransform(Body& body, const ColliderComponent& collider, const TransformComponent& transform)
        {
            const Physics::ColliderShapeWorld shape = Physics::ComputeWorldShape(collider, transform);

            btTransform target;
            target.setIdentity();
            target.setOrigin(ToBullet(shape.Center));
            target.setRotation(ToBullet(shape.Rotation));

            body.RigidBody->setWorldTransform(target);
            body.MotionState->setWorldTransform(target);

            if (body.Dynamic)
            {
                // A teleported dynamic body keeps none of the speed it had at the old place.
                body.RigidBody->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
                body.RigidBody->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
                body.RigidBody->clearForces();
                body.RigidBody->activate(true);
            }
            else
            {
                World.updateSingleAabb(body.RigidBody.get());   // static/kinematic: tell the broadphase it moved
            }

            body.LastPosition = transform.Position;
            body.LastRotation = transform.Rotation;
        }
    };

    PhysicsWorld::PhysicsWorld(const glm::vec3& gravity)
        : m_Impl(std::make_unique<Impl>())
    {
        m_Impl->World.setGravity(ToBullet(gravity));
    }

    PhysicsWorld::~PhysicsWorld()
    {
        m_Impl->Clear();
    }

    void PhysicsWorld::AdoptNewBodies(entt::registry& registry)
    {
        for (auto [entity, transform, collider] : registry.view<TransformComponent, ColliderComponent>().each())
        {
            if (!m_Impl->Bodies.contains(entity))
                m_Impl->CreateBody(registry, entity, transform, collider);
        }
    }

    void PhysicsWorld::RemoveBody(entt::entity entity)
    {
        auto it = m_Impl->Bodies.find(entity);
        if (it == m_Impl->Bodies.end())
            return;

        m_Impl->World.removeRigidBody(it->second.RigidBody.get());
        m_Impl->Bodies.erase(it);
    }

    bool PhysicsWorld::MoveAndSlide(entt::registry& registry, entt::entity entity, const glm::vec3& delta)
    {
        if (!registry.valid(entity) || !registry.all_of<TransformComponent, ColliderComponent>(entity))
            return false;

        AdoptNewBodies(registry);   // an entity created this frame has no body yet, but needs a shape to sweep

        auto it = m_Impl->Bodies.find(entity);
        if (it == m_Impl->Bodies.end() || it->second.Dynamic)
            return false;

        Impl::Body& body = it->second;
        auto& transform = registry.get<TransformComponent>(entity);
        const auto& collider = registry.get<ColliderComponent>(entity);

        // Sweep from where the ENTITY is, not from where the body last was: a script may have moved it already.
        const Physics::ColliderShapeWorld shape = Physics::ComputeWorldShape(collider, transform);
        glm::vec3 center = shape.Center;
        glm::vec3 remaining = delta;
        bool blocked = false;

        constexpr int maxSlides = 4;     // enough for a corner (two walls) with one to spare
        constexpr float skin = 0.01f;    // stay this far off every surface so we never end up touching it

        for (int pass = 0; pass < maxSlides; ++pass)
        {
            const float length = glm::length(remaining);
            if (length < 1e-5f)
                break;
            const glm::vec3 direction = remaining / length;

            btTransform from, to;
            from.setIdentity();
            to.setIdentity();
            from.setRotation(ToBullet(shape.Rotation));
            to.setRotation(ToBullet(shape.Rotation));
            from.setOrigin(ToBullet(center));
            to.setOrigin(ToBullet(center + remaining));

            SolidSweepCallback hit(from.getOrigin(), to.getOrigin(), body.RigidBody.get());
            m_Impl->World.convexSweepTest(body.Convex, from, to, hit);

            if (!hit.hasHit())
            {
                center += remaining;   // nothing in the way: the whole remaining move happens
                remaining = glm::vec3(0.0f);
                break;
            }

            blocked = true;

            /* The sweep says "you can go `fraction` of the way before touching something". Go that far, minus
               the skin (but never backwards, so being already within the skin doesn't push us around). */
            const float fraction = hit.m_closestHitFraction;
            center += direction * std::max(length * fraction - skin, 0.0f);

            /* The part of the movement we didn't get to make, minus the part that points INTO the surface,
               is the part that runs along it. That is the slide. The normal must face against our motion
               (a sweep can report it either way round), so flip it if it doesn't. */
            glm::vec3 normal = ToGlm(hit.m_hitNormalWorld);
            if (glm::dot(normal, direction) > 0.0f)
                normal = -normal;

            glm::vec3 leftover = remaining * (1.0f - fraction);
            leftover -= normal * glm::dot(leftover, normal);
            remaining = leftover;
        }

        // The sweep worked on the collider's centre; the entity's origin is that minus the collider's offset.
        transform.Position = center - shape.Rotation * (collider.Offset * transform.Scale);

        // The body itself catches up on the next Step (it sees the transform changed and moves there), and
        // for a kinematic body that is also what gives it the velocity it uses to push dynamic bodies.
        return blocked;
    }

    void PhysicsWorld::Step(entt::registry& registry, float dt)
    {
        AdoptNewBodies(registry);

        // 1. Entities -> bodies: anything moved from outside the simulation (scripts) is followed.
        for (auto& [entity, body] : m_Impl->Bodies)
        {
            if (!registry.valid(entity) || !registry.all_of<TransformComponent, ColliderComponent>(entity))
                continue;

            const auto& transform = registry.get<TransformComponent>(entity);
            if (transform.Position != body.LastPosition || transform.Rotation != body.LastRotation)
                m_Impl->PushTransform(body, registry.get<ColliderComponent>(entity), transform);
        }

        /* 2. Simulate. Bullet advances in FIXED slices (1/60 s) however long the frame was, which keeps the
           result the same on a fast or slow machine. It runs as many slices as the frame needs (at most 10,
           so one huge hitch can't make it spiral), then interpolates the left-over fraction, which is why
           motion still looks smooth when the frame rate isn't a multiple of 60. */
        m_Impl->World.stepSimulation(dt, 10, 1.0f / 60.0f);

        // 3. Dynamic bodies -> entities.
        for (auto& [entity, body] : m_Impl->Bodies)
        {
            if (!body.Dynamic || !registry.valid(entity) || !registry.all_of<TransformComponent, ColliderComponent>(entity))
                continue;

            btTransform result;
            body.MotionState->getWorldTransform(result);

            const glm::quat rotation = ToGlm(result.getRotation());
            auto& transform = registry.get<TransformComponent>(entity);
            const auto& collider = registry.get<ColliderComponent>(entity);

            // Bullet reports where the collider's CENTRE is; undo the collider's offset to get the entity.
            transform.Rotation = Physics::QuatToEuler(rotation);
            transform.Position = ToGlm(result.getOrigin()) - rotation * (collider.Offset * transform.Scale);

            // We wrote these ourselves, so they must not look like somebody else's edit next frame.
            body.LastPosition = transform.Position;
            body.LastRotation = transform.Rotation;
        }
    }
}
