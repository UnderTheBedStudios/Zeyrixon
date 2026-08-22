#pragma once

#include <typeinfo>
#include <vector>
#include <memory>

class BaseEntity;
class Camera;
class PhysicsComponent;

extern "C" {
    void Engine_Init(void* getProcAddress, const char* assetRoot);
    void Engine_RenderFrame(int fb, int width, int height, const float* viewProj);
    void Engine_SetLight(const float* lightDir, const float* lightColor, const float* viewPos);

    // outLightDir/outLightColor are always 3 floats each. Either may be nullptr to skip it.
    void Engine_GetLight(float* outLightDir, float* outLightColor);

    // Name of the currently loaded world. "" if no world has been loaded (or Engine_SetWorldName
    // hasn't been called yet). Same lifetime contract as Engine_GetEntityName: owned by the
    // Engine, copy it out immediately rather than holding the pointer.
    const char* Engine_GetWorldName();
    void Engine_SetWorldName(const char* name);
    void Engine_Shutdown();

    // Returns the new entity's index (>= 0), or -1 on failure.
    // type: "Camera" constructs a Camera entity; anything else (including "Empty") constructs
    // a plain BaseEntity with no transform/model.
    int Engine_CreateEntity(const char* type, const char* name);

    int Engine_GetEntityCount();

    // Returned pointers are owned by the Engine and stay valid as long as the entity exists —
    // copy into a std::string immediately on the caller side rather than holding onto them.
    const char* Engine_GetEntityName(int index);
    const char* Engine_GetEntityType(int index);

    bool Engine_SetEntityName(int index, const char* newName);
    bool Engine_DeleteEntity(int index);

    BaseEntity* Engine_GetEntity(int index);

    const std::vector<std::unique_ptr<BaseEntity>>& Engine_GetAllEntities();

    // --- Default camera ---
    // The entity Engine_RenderFrame's caller should treat as the "game" camera (as opposed to
    // the editor's free-fly camera). Returns nullptr if none is set, or if the entity that was
    // set has since been deleted.
    Camera* Engine_GetDefaultCamera();

    // index must refer to an entity created with type "Camera" (see Engine_CreateEntity).
    // Returns false (leaving the current default camera unchanged) for an out-of-range index
    // or an entity that isn't a Camera.
    bool Engine_SetDefaultCamera(int index);

    // Clears the default camera pointer without needing a valid replacement index.
    void Engine_ClearDefaultCamera();

    // --- World load/save ---
    // Deletes all existing entities and clears the default camera, then parses the .zworld
    // file at path and spawns everything it describes (entities, light, default camera).
    // Returns false if the file can't be parsed; entity state is left cleared either way,
    // so check the return value before assuming anything was loaded.
    bool Engine_LoadWorld(const char* path);

    // Snapshots all current entities, light, and the default camera to a .zworld file at path.
    bool Engine_SaveWorld(const char* path);

    // --- Physics ---
    // Creates the physics world (broadphase/dispatcher/solver/btDiscreteDynamicsWorld).
    // gravity is 3 floats; pass nullptr to use the default (0, -9.81, 0). Called automatically
    // at the end of Engine_Init, so you normally don't need to call this yourself — it's
    // exposed separately in case you want to reset gravity after init.
    void Engine_InitPhysics(const float* gravity);

    // Advances the physics simulation by deltaTime seconds, then syncs every entity's
    // PhysicsComponent(s) back into their TransformComponent so rendering sees the new pose.
    // Call exactly once per frame, before Engine_RenderFrame. No-op if Engine_InitPhysics
    // hasn't run yet.
    void Engine_StepPhysics(float deltaTime);

    // --- Physics component wiring ---
    // Construct and Init() a PhysicsComponent directly (e.g. via
    // entity->AddComponent<PhysicsComponent>() on a real BaseEntity* from Engine_GetEntity),
    // then call this to hook its rigid body into the live simulation — a PhysicsComponent
    // that exists but was never registered here is never simulated.
    void Engine_RegisterPhysicsBody(PhysicsComponent* physics);

    // Call before destroying/removing a PhysicsComponent (e.g. via BaseEntity::RemoveComponent),
    // or the physics world is left holding a pointer into memory about to be freed.
    void Engine_UnregisterPhysicsBody(PhysicsComponent* physics);
}

template <typename T, typename ObjType>
bool Engine_EntityIs(const ObjType& obj) { return typeid(obj) == typeid(T); }
