#include <pch.h>

#include <Zeyrixon/Physics/PhysicsWorld.h>
#include <Zeyrixon/Physics/Collider.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/Core/Log.h>

#include <btBulletDynamicsCommon.h>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
           the body goes first, then the motion state, the compound shape, and finally the shapes inside it. */
        struct Body
        {
            // One shape per collider slot. All of them are convex (box, sphere, capsule), which is what lets
            // MoveAndSlide sweep them.
            std::vector<std::unique_ptr<btCollisionShape>> Children;
            std::vector<btConvexShape*> Convex;                // the same objects as Children, as convex shapes

            // With 2+ slots the body's shape is a compound that holds the children; with one slot it is just
            // that child, exactly as it was before slots existed.
            std::unique_ptr<btCompoundShape> Compound;
            btCollisionShape* Shape = nullptr;                 // what the rigid body actually uses

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
        std::unordered_set<uint64_t> Contacts;
        std::vector<CollisionEvent> CollisionEvents;

        static uint64_t PairKey(entt::entity a, entt::entity b)
        {
            uint32_t x = static_cast<uint32_t>(a), y = static_cast<uint32_t>(b);
            if (x > y) std::swap(x, y);
            return (uint64_t(x) << 32) | y;
        }

        // Bodies must be taken OUT of the world before they are destroyed. btDiscreteDynamicsWorld's own
        // destructor walks the bodies still inside it, so freeing one first is a use-after-free.
        void Clear()
        {
            for (auto& [entity, body] : Bodies)
                World.removeRigidBody(body.RigidBody.get());
            Bodies.clear();
        }

        /* The body is placed at the collider's CENTRE OF MASS, not at the entity's origin, and each slot is
           placed relative to that. For one slot the centre of mass is the slot's own centre, so the body sits
           exactly on the shape; for several it is their volume-weighted average. The entity's position is
           always `body position - rotation * centreOfMass`. */
        glm::vec3 BodyOrigin(const ColliderComponent& collider, const TransformComponent& transform) const
        {
            const glm::quat rotation = Physics::EulerToQuat(transform.Rotation);
            return transform.Position + rotation * Physics::ComputeCenterOfMassOffset(collider, transform);
        }

        void CreateBody(entt::registry& registry, entt::entity entity,
                        const TransformComponent& transform, const ColliderComponent& collider)
        {
            Body body;

            const glm::vec3 centerOfMass = Physics::ComputeCenterOfMassOffset(collider, transform);

            for (const ColliderSlot& slot : collider.Slots)
            {
                body.Children.push_back(MakeShape(slot.Shape, Physics::ComputeWorldShape(slot, transform)));
                body.Convex.push_back(static_cast<btConvexShape*>(body.Children.back().get()));   // box, sphere, capsule: all convex
            }

            if (body.Children.size() == 1)
            {
                body.Shape = body.Children[0].get();
            }
            else
            {
                body.Compound = std::make_unique<btCompoundShape>();
                for (size_t i = 0; i < body.Children.size(); ++i)
                {
                    // Each child sits at its own centre, measured from the centre of mass. The body's rotation
                    // is the entity's, so the entity's own axes are the body's axes and no rotation is needed here.
                    btTransform local;
                    local.setIdentity();
                    local.setOrigin(ToBullet(collider.Slots[i].Offset * transform.Scale - centerOfMass));
                    body.Compound->addChildShape(local, body.Children[i].get());
                }
                body.Shape = body.Compound.get();
            }

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
            start.setOrigin(ToBullet(BodyOrigin(collider, transform)));
            start.setRotation(ToBullet(Physics::EulerToQuat(transform.Rotation)));
            body.MotionState = std::make_unique<btDefaultMotionState>(start);

            btRigidBody::btRigidBodyConstructionInfo info(mass, body.MotionState.get(), body.Shape, inertia);
            if (settings)
            {
                info.m_friction = settings->Friction;
                info.m_restitution = settings->Restitution;
                info.m_linearDamping = settings->LinearDamping;
                info.m_angularDamping = settings->AngularDamping;
            }
            body.RigidBody = std::make_unique<btRigidBody>(info);
            body.RigidBody->setUserIndex(static_cast<int>(entity));

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
            btTransform target;
            target.setIdentity();
            target.setOrigin(ToBullet(BodyOrigin(collider, transform)));
            target.setRotation(ToBullet(Physics::EulerToQuat(transform.Rotation)));

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
            // A collider with no slots has no shape, so there is nothing to simulate.
            if (!collider.Slots.empty() && !m_Impl->Bodies.contains(entity))
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

    RaycastHit PhysicsWorld::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) const
    {
        RaycastHit result;
        const float length = glm::length(direction);
        if (length <= 1e-6f || maxDistance <= 0.0f) return result;
        const btVector3 from = ToBullet(origin);
        const btVector3 to = ToBullet(origin + direction / length * maxDistance);
        btCollisionWorld::ClosestRayResultCallback callback(from, to);
        m_Impl->World.rayTest(from, to, callback);
        if (!callback.hasHit()) return result;
        result.Hit = true;
        result.Point = ToGlm(callback.m_hitPointWorld);
        result.Normal = ToGlm(callback.m_hitNormalWorld);
        result.Distance = maxDistance * callback.m_closestHitFraction;
        result.Entity = static_cast<entt::entity>(callback.m_collisionObject->getUserIndex());
        return result;
    }

    const std::vector<CollisionEvent>& PhysicsWorld::GetCollisionEvents() const { return m_Impl->CollisionEvents; }

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

        /* Every slot is swept, and the CLOSEST hit among them wins: a character whose head sphere would hit a
           low ceiling stops there even though its capsule body would have fit. All slots move together, so a
           single displacement describes where they all are. Slots added after Play started have no shape yet
           and are skipped. */
        const glm::quat rotation = Physics::EulerToQuat(transform.Rotation);
        const size_t slotCount = std::min(body.Convex.size(), collider.Slots.size());

        // Sweep from where the ENTITY is, not from where the body last was: a script may have moved it already.
        std::vector<glm::vec3> startCenters(slotCount);
        for (size_t i = 0; i < slotCount; ++i)
            startCenters[i] = Physics::ComputeWorldShape(collider.Slots[i], transform).Center;

        glm::vec3 displacement(0.0f);
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

            // The earliest thing any slot would run into.
            float closestFraction = 2.0f;
            glm::vec3 closestNormal(0.0f);
            for (size_t i = 0; i < slotCount; ++i)
            {
                btTransform from, to;
                from.setIdentity();
                to.setIdentity();
                from.setRotation(ToBullet(rotation));
                to.setRotation(ToBullet(rotation));
                from.setOrigin(ToBullet(startCenters[i] + displacement));
                to.setOrigin(ToBullet(startCenters[i] + displacement + remaining));

                SolidSweepCallback hit(from.getOrigin(), to.getOrigin(), body.RigidBody.get());
                m_Impl->World.convexSweepTest(body.Convex[i], from, to, hit);

                if (hit.hasHit() && hit.m_closestHitFraction < closestFraction)
                {
                    closestFraction = hit.m_closestHitFraction;
                    closestNormal = ToGlm(hit.m_hitNormalWorld);
                }
            }

            if (closestFraction > 1.0f)
            {
                displacement += remaining;   // nothing in the way: the whole remaining move happens
                remaining = glm::vec3(0.0f);
                break;
            }

            blocked = true;

            /* The sweep says "you can go `fraction` of the way before touching something". Go that far, minus
               the skin (but never backwards, so being already within the skin doesn't push us around). */
            displacement += direction * std::max(length * closestFraction - skin, 0.0f);

            /* The part of the movement we didn't get to make, minus the part that points INTO the surface,
               is the part that runs along it. That is the slide. The normal must face against our motion
               (a sweep can report it either way round), so flip it if it doesn't. */
            glm::vec3 normal = closestNormal;
            if (glm::dot(normal, direction) > 0.0f)
                normal = -normal;

            glm::vec3 leftover = remaining * (1.0f - closestFraction);
            leftover -= normal * glm::dot(leftover, normal);
            remaining = leftover;
        }

        // Every slot moved by the same amount, so the entity did too.
        transform.Position += displacement;

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

        m_Impl->CollisionEvents.clear();
        std::unordered_set<uint64_t> currentContacts;
        for (int i = 0; i < m_Impl->Dispatcher.getNumManifolds(); ++i)
        {
            const btPersistentManifold* manifold = m_Impl->Dispatcher.getManifoldByIndexInternal(i);
            bool touching = false;
            for (int p = 0; p < manifold->getNumContacts(); ++p)
                if (manifold->getContactPoint(p).getDistance() <= 0.0f) { touching = true; break; }
            if (!touching) continue;
            const auto* a = static_cast<const btCollisionObject*>(manifold->getBody0());
            const auto* b = static_cast<const btCollisionObject*>(manifold->getBody1());
            const auto first = static_cast<entt::entity>(a->getUserIndex());
            const auto second = static_cast<entt::entity>(b->getUserIndex());
            const uint64_t key = Impl::PairKey(first, second);
            currentContacts.insert(key);
            if (!m_Impl->Contacts.contains(key)) m_Impl->CollisionEvents.push_back({first, second, true});
        }
        for (uint64_t key : m_Impl->Contacts)
            if (!currentContacts.contains(key))
                m_Impl->CollisionEvents.push_back({static_cast<entt::entity>(key >> 32), static_cast<entt::entity>(key), false});
        m_Impl->Contacts = std::move(currentContacts);

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

            // Bullet reports where the centre of mass is; undo that offset to get the entity's origin.
            transform.Rotation = Physics::QuatToEuler(rotation);
            transform.Position = ToGlm(result.getOrigin()) - rotation * Physics::ComputeCenterOfMassOffset(collider, transform);

            // We wrote these ourselves, so they must not look like somebody else's edit next frame.
            body.LastPosition = transform.Position;
            body.LastRotation = transform.Rotation;
        }
    }
}
