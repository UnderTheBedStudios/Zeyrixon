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
}