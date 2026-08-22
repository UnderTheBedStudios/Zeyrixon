#include <Engine/Public/Engine.h>
#include <Engine/pch.h>
#include <Engine/framework.h>
#include <Engine/Public/General/Shader.h>
#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Entities/Common/Camera.h>
#include <Engine/Public/Components/Common/Model.h>
#include <Engine/Public/Entities/Shapes/Cube.h>
#include <Engine/Public/Entities/Shapes/Sphere.h>
#include <Engine/Public/Common/World.h>
#include <Engine/Public/Components/Common/PhysicsComponent.h>
#include <glad/glad.h>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <vector>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>
#include <btBulletDynamicsCommon.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

typedef void* (*GLADloadproc)(const char* name);

namespace {

std::vector<std::unique_ptr<BaseEntity>> g_Entities;

// Raw pointer, not an index: erasing earlier elements from g_Entities shifts the
// unique_ptr *wrappers* around in the vector's storage, but each entity itself is a
// separate heap allocation the wrapper points to, so this pointer stays valid across
// unrelated erases. It's only invalidated by deleting this exact entity (handled in
// Engine_DeleteEntity below).
Camera* g_DefaultCamera = nullptr;

BaseEntity* GetEntitySafe(int index)
{
    if (index < 0 || index >= (int)g_Entities.size())
        return nullptr;
    return g_Entities[index].get();
}

std::unique_ptr<Shader> g_MainShader;
std::unique_ptr<Shader> g_DepthShader;

glm::vec3 g_LightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
glm::vec3 g_LightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 g_ViewPos    = glm::vec3(0.0f);

std::string g_WorldName;

std::string g_AssetRoot;

// --- Physics globals ---
// Declaration order matters: these are destroyed in reverse declaration order when the
// program exits, and btDiscreteDynamicsWorld must be torn down before the dispatcher/config
// it was built from. Engine_Shutdown() also does this explicitly and in the same order, so
// don't rely on static destruction order alone — that only covers the process-exit case.
std::unique_ptr<btDefaultCollisionConfiguration> g_CollisionConfig;
std::unique_ptr<btCollisionDispatcher> g_Dispatcher;
std::unique_ptr<btBroadphaseInterface> g_Broadphase;
std::unique_ptr<btSequentialImpulseConstraintSolver> g_Solver;
std::unique_ptr<btDiscreteDynamicsWorld> g_PhysicsWorld;

// Removes every entity's rigid body from g_PhysicsWorld without touching g_Entities itself.
// Must run before any g_Entities.clear()/erase() that destroys entities still holding a
// PhysicsComponent — otherwise btDiscreteDynamicsWorld is left holding pointers into freed
// btRigidBody objects, which crashes the next stepSimulation rather than at the point of the
// actual bug. This was a real gap in the step-2 debug-box version: it never had more than one
// body, so a whole-world reload (Engine_LoadWorld, Engine_Shutdown) never exercised this path.
void RemoveAllPhysicsBodiesFromWorld()
{
    if (!g_PhysicsWorld)
        return;
    for (std::unique_ptr<BaseEntity>& entity : g_Entities)
    {
        if (PhysicsComponent* physics = entity->GetPhysics())
            g_PhysicsWorld->removeRigidBody(physics->GetRigidBody());
    }
}

// Pushes an entity's current TransformComponent into its physics body, if it has one — used
// whenever something external (Inspector drag, Engine_AddPhysicsComponent's initial pose)
// changes the transform out from under Bullet.
void SyncPhysicsFromEntityTransform(BaseEntity* entity)
{
    PhysicsComponent* physics = entity->GetPhysics();
    if (!physics)
        return;

    Transform t;
    t.Position = entity->GetTransform()->GetPosition();
    t.Rotation = entity->GetTransform()->GetQuaternion();
    t.Scale = entity->GetTransform()->GetScale();
    physics->SyncTransformToPhysics(t);
}

// path may be an absolute filesystem path, or relative to the asset root. Existing callers
// (Camera/PrimativeShape ctors) always pass assetRoot + "/Engine/..." themselves, so this only
// needs to handle what the editor UI hands in.
std::string ResolveModelPath(const std::string& path)
{
    if (!path.empty() && path[0] == '/')
    {
        if (access(path.c_str(), F_OK) == 0)
            return path;
        return g_AssetRoot + path;
    }
    return g_AssetRoot + "/" + path;
}

GLuint g_ShadowFBO = 0;
GLuint g_ShadowMap = 0;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

glm::mat4 g_LightSpaceMatrix = glm::mat4(1.0f);

const char* vertexShaderPath        = "/Engine/Shaders/basic.vert";
const char* fragmentShaderPath      = "/Engine/Shaders/basic.frag";
const char* depthVertexShaderPath   = "/Engine/Shaders/depth.vert";
const char* depthFragmentShaderPath = "/Engine/Shaders/depth.frag";

void InitShadowMap()
{
    // unchanged — see prior version
    glGenFramebuffers(1, &g_ShadowFBO);

    glGenTextures(1, &g_ShadowMap);
    glBindTexture(GL_TEXTURE_2D, g_ShadowMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, g_ShadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_ShadowMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(stderr, "[Engine] Shadow FBO incomplete\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void UpdateLightSpaceMatrix()
{
    float sceneRadius = 10.0f; // TODO: derive from real scene bounds once you have more objects

    glm::vec3 lightPos = g_LightDir * sceneRadius;
    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProj = glm::ortho(-sceneRadius, sceneRadius, -sceneRadius, sceneRadius, 0.1f, sceneRadius * 2.0f);

    g_LightSpaceMatrix = lightProj * lightView;
}

} // anonymous namespace

extern "C" {

void Engine_Shutdown()
{
    glDeleteTextures(1, &g_ShadowMap);
    glDeleteFramebuffers(1, &g_ShadowFBO);

    g_MainShader.reset();
    g_DepthShader.reset();

    // Must happen before g_Entities.clear(): otherwise any entity holding a PhysicsComponent
    // destroys its btRigidBody while g_PhysicsWorld still has an internal pointer to it.
    RemoveAllPhysicsBodiesFromWorld();
    g_Entities.clear();
    g_DefaultCamera = nullptr;
    g_WorldName.clear();

    // Physics teardown, in dependency order — world depends on dispatcher+config+broadphase+
    // solver, so it must go first.
    g_PhysicsWorld.reset();
    g_Solver.reset();
    g_Broadphase.reset();
    g_Dispatcher.reset();
    g_CollisionConfig.reset();
}

void Engine_Init(void* getProcAddress, const char* assetRoot)
{
    if (!gladLoadGLLoader((GLADloadproc)getProcAddress))
    {
        fprintf(stderr, "[Engine] Failed to initialize GLAD\n");
        return;
    }
    fprintf(stderr, "[Engine] GLAD initialized, GL version %s\n", glGetString(GL_VERSION));

    glEnable(GL_DEPTH_TEST);

    g_AssetRoot = assetRoot;

    std::string vertPath  = g_AssetRoot + vertexShaderPath;
    std::string fragPath  = g_AssetRoot + fragmentShaderPath;
    std::string dVertPath = g_AssetRoot + depthVertexShaderPath;
    std::string dFragPath = g_AssetRoot + depthFragmentShaderPath;

    g_MainShader  = std::make_unique<Shader>(vertPath.c_str(), fragPath.c_str());
    g_DepthShader = std::make_unique<Shader>(dVertPath.c_str(), dFragPath.c_str());

    InitShadowMap();
    UpdateLightSpaceMatrix();

    Engine_InitPhysics(nullptr);
}

int Engine_CreateEntity(const char* type, const char* name)
{
    std::unique_ptr<BaseEntity> entity;

    std::string typeStr = type ? type : "";
    if (typeStr == "Camera")
        entity = std::make_unique<Camera>(g_AssetRoot);
    else if (typeStr == "Cube")
        entity = std::make_unique<Cube>(g_AssetRoot);
    else if (typeStr == "Sphere")
        entity = std::make_unique<Sphere>(g_AssetRoot);
    else
        entity = std::make_unique<BaseEntity>();

    entity->Name = (name && name[0] != '\0') ? name : entity->GetTypeName();

    g_Entities.push_back(std::move(entity));
    return (int)g_Entities.size() - 1;
}

int Engine_GetEntityCount()
{
    return (int)g_Entities.size();
}

const char* Engine_GetEntityName(int index)
{
    if (index < 0 || index >= (int)g_Entities.size())
        return "";
    return g_Entities[index]->Name.c_str();
}

const char* Engine_GetEntityType(int index)
{
    if (index < 0 || index >= (int)g_Entities.size())
        return "";
    return g_Entities[index]->GetTypeName();
}

bool Engine_SetEntityName(int index, const char* newName)
{
    if (index < 0 || index >= (int)g_Entities.size())
        return false;
    g_Entities[index]->Name = newName ? newName : "";
    return true;
}

bool Engine_DeleteEntity(int index)
{
    if (index < 0 || index >= (int)g_Entities.size())
        return false;
    if (g_Entities[index].get() == g_DefaultCamera)
        g_DefaultCamera = nullptr;
    // Same reasoning as RemoveAllPhysicsBodiesFromWorld: the entity (and its PhysicsComponent)
    // is about to be destroyed by the erase below, so its body must leave g_PhysicsWorld first.
    if (PhysicsComponent* physics = g_Entities[index]->GetPhysics())
    {
        if (g_PhysicsWorld)
            g_PhysicsWorld->removeRigidBody(physics->GetRigidBody());
    }
    g_Entities.erase(g_Entities.begin() + index);
    return true;
}

bool Engine_GetEntityPosition(int index, float* outXYZ)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !outXYZ)
        return false;
    glm::vec3 p = entity->GetTransform()->GetPosition();
    outXYZ[0] = p.x; outXYZ[1] = p.y; outXYZ[2] = p.z;
    return true;
}

bool Engine_SetEntityPosition(int index, const float* xyz)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !xyz)
        return false;
    entity->GetTransform()->SetPosition(glm::vec3(xyz[0], xyz[1], xyz[2]));
    SyncPhysicsFromEntityTransform(entity);
    return true;
}

bool Engine_GetEntityRotation(int index, float* outXYZ)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !outXYZ)
        return false;
    glm::vec3 r = entity->GetTransform()->GetRotation();
    outXYZ[0] = r.x; outXYZ[1] = r.y; outXYZ[2] = r.z;
    return true;
}

bool Engine_SetEntityRotation(int index, const float* xyz)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !xyz)
        return false;
    entity->GetTransform()->SetRotation(glm::vec3(xyz[0], xyz[1], xyz[2]));
    SyncPhysicsFromEntityTransform(entity);
    return true;
}

bool Engine_GetEntityScale(int index, float* outXYZ)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !outXYZ)
        return false;
    glm::vec3 s = entity->GetTransform()->GetScale();
    outXYZ[0] = s.x; outXYZ[1] = s.y; outXYZ[2] = s.z;
    return true;
}

bool Engine_SetEntityScale(int index, const float* xyz)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !xyz)
        return false;
    entity->GetTransform()->SetScale(glm::vec3(xyz[0], xyz[1], xyz[2]));
    return true;
}

BaseEntity* Engine_GetEntity(int index)
{
    return GetEntitySafe(index);
}

const std::vector<std::unique_ptr<BaseEntity>>& Engine_GetAllEntities()
{
    return g_Entities;
}

bool Engine_EntityHasModel(int index)
{
    BaseEntity* entity = GetEntitySafe(index);
    return entity && entity->GetModel() != nullptr;
}

const char* Engine_GetEntityModelPath(int index)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !entity->GetModel())
        return "";
    return entity->GetModel()->GetPath().c_str();
}

bool Engine_SetEntityModelPath(int index, const char* path)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !entity->GetModel() || !path)
        return false;
    return entity->GetModel()->LoadFromFile(ResolveModelPath(path));
}

const char* Engine_GetWorldName()
{
    return g_WorldName.c_str();
}

void Engine_SetWorldName(const char* name)
{
    g_WorldName = name ? name : "";
}

Camera* Engine_GetDefaultCamera()
{
    return g_DefaultCamera;
}

bool Engine_SetDefaultCamera(int index)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !Engine_EntityIs<Camera>(*entity))
        return false;
    g_DefaultCamera = dynamic_cast<Camera*>(entity);
    return true;
}

void Engine_ClearDefaultCamera()
{
    g_DefaultCamera = nullptr;
}

bool Engine_LoadWorld(const char* path)
{
    World world;
    if (!path || !world.LoadFromFile(path))
    {
        // Matches the "state left cleared either way" contract documented in Engine.h —
        // a half-loaded world is worse than an empty one, since the caller has no way to
        // tell which entities came from the old world vs. a partially-parsed new one.
        RemoveAllPhysicsBodiesFromWorld(); // must run before clear() — see its own comment
        g_Entities.clear();
        g_DefaultCamera = nullptr;
        return false;
    }

    RemoveAllPhysicsBodiesFromWorld();
    g_Entities.clear();
    g_DefaultCamera = nullptr;

    g_WorldName = world.Name();
    g_LightDir = world.LightDir();
    g_LightColor = world.LightColor();
    UpdateLightSpaceMatrix();

    for (const WorldEntity& e : world.Entities())
    {
        int index = Engine_CreateEntity(e.type.c_str(), e.name.c_str());
        if (index < 0)
            continue;

        Engine_SetEntityPosition(index, glm::value_ptr(e.position));
        Engine_SetEntityRotation(index, glm::value_ptr(e.rotation));
        Engine_SetEntityScale(index, glm::value_ptr(e.scale));

        if (e.hasModel && Engine_EntityHasModel(index))
            Engine_SetEntityModelPath(index, e.modelPath.c_str());

        if (!world.DefaultCameraName().empty() && e.name == world.DefaultCameraName())
            Engine_SetDefaultCamera(index);
    }

    return true;
}

bool Engine_SaveWorld(const char* path)
{
    if (!path)
        return false;

    World world;
    world.SetName(g_WorldName);
    world.SetLightDir(g_LightDir);
    world.SetLightColor(g_LightColor);
    world.SetDefaultCameraName(g_DefaultCamera ? g_DefaultCamera->Name : std::string());

    std::vector<WorldEntity>& entities = world.Entities();
    entities.reserve(g_Entities.size());
    for (const std::unique_ptr<BaseEntity>& entity : g_Entities)
    {
        WorldEntity e;
        e.type = entity->GetTypeName();
        e.name = entity->Name;
        e.position = entity->GetTransform()->GetPosition();
        e.rotation = entity->GetTransform()->GetRotation();
        e.scale = entity->GetTransform()->GetScale();
        e.hasModel = entity->GetModel() != nullptr;
        e.modelPath = e.hasModel ? entity->GetModel()->GetPath() : std::string();
        entities.push_back(std::move(e));
    }

    return world.SaveToFile(path);
}

void Engine_InitPhysics(const float* gravity)
{
    g_CollisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
    g_Dispatcher = std::make_unique<btCollisionDispatcher>(g_CollisionConfig.get());
    g_Broadphase = std::make_unique<btDbvtBroadphase>();
    g_Solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    g_PhysicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
        g_Dispatcher.get(), g_Broadphase.get(), g_Solver.get(), g_CollisionConfig.get());

    glm::vec3 g = gravity ? glm::vec3(gravity[0], gravity[1], gravity[2]) : glm::vec3(0.0f, -9.81f, 0.0f);
    g_PhysicsWorld->setGravity(btVector3(g.x, g.y, g.z));

    fprintf(stderr, "[Engine] Physics world initialized (gravity = %.2f, %.2f, %.2f)\n", g.x, g.y, g.z);
}

void Engine_StepPhysics(float deltaTime)
{
    if (!g_PhysicsWorld)
        return;

    g_PhysicsWorld->stepSimulation(deltaTime, 10);

    for (std::unique_ptr<BaseEntity>& entity : g_Entities)
    {
        if (PhysicsComponent* physics = entity->GetPhysics())
            physics->SyncPhysicsToTransform(entity->GetTransform());
    }
}

bool Engine_AddPhysicsComponent(int index, int shapeType, float mass, const float* dims)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !dims || !g_PhysicsWorld)
        return false;

    // Replacing an existing component: pull its body out of the world first. Just letting
    // SetPhysics() overwrite the unique_ptr would destroy the old btRigidBody while it's
    // still registered in g_PhysicsWorld — same class of dangling-pointer bug fixed above.
    if (PhysicsComponent* existing = entity->GetPhysics())
        g_PhysicsWorld->removeRigidBody(existing->GetRigidBody());

    auto physics = std::make_unique<PhysicsComponent>();
    physics->Init(static_cast<PhysicsComponent::ShapeType>(shapeType), mass,
                  glm::vec3(dims[0], dims[1], dims[2]));

    g_PhysicsWorld->addRigidBody(physics->GetRigidBody());
    entity->SetPhysics(std::move(physics));

    // Init() starts the body at the identity transform — snap it to where the entity
    // actually is right now instead of teleporting the visual mesh to match it next frame.
    SyncPhysicsFromEntityTransform(entity);
    return true;
}

bool Engine_RemovePhysicsComponent(int index)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity)
        return false;

    if (PhysicsComponent* existing = entity->GetPhysics())
    {
        if (g_PhysicsWorld)
            g_PhysicsWorld->removeRigidBody(existing->GetRigidBody());
        entity->ClearPhysics();
    }
    return true;
}

bool Engine_EntityHasPhysics(int index)
{
    BaseEntity* entity = GetEntitySafe(index);
    return entity && entity->GetPhysics() != nullptr;
}

bool Engine_GetBodyMass(int index, float* outMass)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !entity->GetPhysics() || !outMass)
        return false;
    *outMass = entity->GetPhysics()->GetMass();
    return true;
}

bool Engine_SetBodyMass(int index, float mass)
{
    BaseEntity* entity = GetEntitySafe(index);
    if (!entity || !entity->GetPhysics())
        return false;
    entity->GetPhysics()->SetMass(mass);
    return true;
}

void Engine_RenderFrame(int fb, int width, int height, const float* viewProj)
{
    // --- Pass 1: render depth from the light's POV ---
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_ShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    g_DepthShader->use();
    g_DepthShader->setMat4("uLightSpaceMatrix", g_LightSpaceMatrix);

    for (auto& entity : g_Entities)
    {
        Model* model = entity->GetModel();
        TransformComponent* transform = entity->GetTransform();
        if (!model || !transform)
            continue;

        g_DepthShader->setMat4("uModel", transform->GetModelMatrix());
        model->Draw();
    }

    glDisable(GL_CULL_FACE);

    // --- Pass 2: normal scene render, sampling the shadow map ---
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, width, height);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    g_MainShader->use();
    g_MainShader->setMat4("uViewProj", glm::make_mat4(viewProj));
    g_MainShader->setMat4("uLightSpaceMatrix", g_LightSpaceMatrix);
    g_MainShader->setVec3("uLightDir", g_LightDir);
    g_MainShader->setVec3("uLightColor", g_LightColor);
    g_MainShader->setVec3("uViewPos", g_ViewPos);
    g_MainShader->setVec4("vertexColor", glm::vec4(1.0f));
    g_MainShader->setVec4("ambientLightColor", glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));

    g_MainShader->setInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_ShadowMap);
    g_MainShader->setInt("uShadowMap", 1);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (auto& entity : g_Entities)
    {
        Model* model = entity->GetModel();
        TransformComponent* transform = entity->GetTransform();
        if (!model || !transform)
            continue;

        g_MainShader->setMat4("uModel", transform->GetModelMatrix());
        model->Draw();
    }
}

void Engine_SetLight(const float* lightDir, const float* lightColor, const float* viewPos)
{
    g_LightDir   = glm::normalize(glm::vec3(lightDir[0], lightDir[1], lightDir[2]));
    g_LightColor = glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
    g_ViewPos    = glm::vec3(viewPos[0], viewPos[1], viewPos[2]);
    UpdateLightSpaceMatrix();
}

void Engine_GetLight(float* outLightDir, float* outLightColor)
{
    if (outLightDir)
    {
        outLightDir[0] = g_LightDir.x; outLightDir[1] = g_LightDir.y; outLightDir[2] = g_LightDir.z;
    }
    if (outLightColor)
    {
        outLightColor[0] = g_LightColor.x; outLightColor[1] = g_LightColor.y; outLightColor[2] = g_LightColor.z;
    }
}

} // extern "C"
