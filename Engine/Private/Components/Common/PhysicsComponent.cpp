#include <Engine/Public/Components/Common/PhysicsComponent.h>

namespace
{
    // Helper: build the right btCollisionShape for a given ShapeType + dimensions.
    // halfExtentsOrRadius is interpreted per-shape:
    //   Box          -> half-extents (x, y, z)
    //   Sphere       -> radius in .x
    //   Capsule      -> radius in .x, half-height in .y
    //   ConvexHall   -> not yet supported here (needs mesh data, see note below)
    //   StaticPlane  -> plane normal in (x, y, z), no offset (assumes plane through origin)
    std::unique_ptr<btCollisionShape> MakeShape(PhysicsComponent::ShapeType type, const glm::vec3& dims)
    {
        switch (type)
        {
            case PhysicsComponent::ShapeType::Box:
                return std::make_unique<btBoxShape>(btVector3(dims.x, dims.y, dims.z));
            case PhysicsComponent::ShapeType::Sphere:
                return std::make_unique<btSphereShape>(dims.x);
            case PhysicsComponent::ShapeType::Capsule:
                return std::make_unique<btCapsuleShape>(dims.x, dims.y);
            case PhysicsComponent::ShapeType::StaticPlane:
                return std::make_unique<btStaticPlaneShape>(btVector3(dims.x, dims.y, dims.z), 0.0f);
            case PhysicsComponent::ShapeType::ConvexHall:
                // TODO: needs mesh vertex data from Model/Mesh, not just a glm::vec3. Placeholder
                // box until that plumbing exists so this at least doesn't crash if selected.
                return std::make_unique<btBoxShape>(btVector3(dims.x, dims.y, dims.z));
        }
        return nullptr;
    }
}

void PhysicsComponent::Init(ShapeType shape, float mass, glm::vec3 halfExtentsOrRadius)
{
    m_Shape = MakeShape(shape, halfExtentsOrRadius);

    btTransform startTransform;
    startTransform.setIdentity();

    m_MotionState = std::make_unique<btDefaultMotionState>(startTransform);

    // Static/kinematic-only shapes (mass == 0) must report zero local inertia, matching
    // the smoke test's ground plane. Bullet requires this or the body won't behave as static.
    btVector3 localInertia(0.f, 0.f, 0.f);
    if (mass > 0.0f)
        m_Shape->calculateLocalInertia(mass, localInertia);

    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, m_MotionState.get(), m_Shape.get(), localInertia);
    m_Body = std::make_unique<btRigidBody>(rbInfo);
}

void PhysicsComponent::SyncTransformToPhysics(const Transform& transform)
{
    if (!m_Body)
        return;

    btTransform btTrans;
    btTrans.setOrigin(btVector3(transform.Position.x, transform.Position.y, transform.Position.z));
    btTrans.setRotation(btQuaternion(transform.Rotation.x, transform.Rotation.y, transform.Rotation.z, transform.Rotation.w));

    // Waking the body up matters here: an editor drag on a sleeping body would otherwise
    // be silently ignored by Bullet until something else disturbs it.
    m_Body->setWorldTransform(btTrans);
    if (m_MotionState)
        m_MotionState->setWorldTransform(btTrans);
    m_Body->activate(true);
}

void PhysicsComponent::SyncPhysicsToTransform(TransformComponent* transform)
{
    if (!m_Body || !m_MotionState || !transform)
        return;

    btTransform btTrans;
    m_MotionState->getWorldTransform(btTrans);

    const btVector3& origin = btTrans.getOrigin();
    const btQuaternion rot = btTrans.getRotation();

    transform->SetPosition(glm::vec3(origin.getX(), origin.getY(), origin.getZ()));
    transform->SetQuaternion(glm::quat(rot.getW(), rot.getX(), rot.getY(), rot.getZ()));
}
