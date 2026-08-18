#pragma once

extern "C" {
    void Engine_Init(void* getProcAddress, const char* assetRoot);
    void Engine_RenderFrame(int fb, int width, int height, const float* viewProj);
    void Engine_SetLight(const float* lightDir, const float* lightColor, const float* viewPos);
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
}