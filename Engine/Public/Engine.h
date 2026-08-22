#pragma once

#include <typeinfo>
#include <vector>
#include <memory>

class BaseEntity;
class Camera;

extern "C" {
	void Engine_InitPhysics(const float* gravity);
	void Engine_StepPhysics(float deltaTime);

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

    // --- Transform component ---
    // outXYZ/xyz are always 3 floats. Rotation is Euler degrees (matches
    // TransformComponent::SetRotation/GetRotation, which store the quaternion internally).
    // All return false (leaving outXYZ untouched) if index is out of range.
    bool Engine_GetEntityPosition(int index, float* outXYZ);
    bool Engine_SetEntityPosition(int index, const float* xyz);
    bool Engine_GetEntityRotation(int index, float* outXYZ);
    bool Engine_SetEntityRotation(int index, const float* xyz);
    bool Engine_GetEntityScale(int index, float* outXYZ);
    bool Engine_SetEntityScale(int index, const float* xyz);

    BaseEntity* Engine_GetEntity(int index);

    const std::vector<std::unique_ptr<BaseEntity>>& Engine_GetAllEntities();


    // --- Model component ---
    // false for entities with no model slot (e.g. a plain "Empty Entity") — GetModel()
    // returns nullptr for those, and this API does not add one.
    bool Engine_EntityHasModel(int index);

    // Path last successfully loaded, or "" if none has loaded yet. Same lifetime contract
    // as Engine_GetEntityName: owned by the Engine, valid as long as the entity exists.
    const char* Engine_GetEntityModelPath(int index);

    // path may be absolute, or relative to the asset root (e.g. "/Engine/Models/Cube/cube.obj").
    // Reloads the entity's existing Model in place; fails (returns false) if the entity has
    // no model slot or the file can't be loaded, leaving the previous model intact.
    bool Engine_SetEntityModelPath(int index, const char* path);

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
}

template <typename T, typename ObjType>
bool Engine_EntityIs(const ObjType& obj) { return typeid(obj) == typeid(T); }
