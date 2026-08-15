#pragma once

extern "C" {
    void Engine_Init(void* getProcAddress, const char* assetRoot);
    void Engine_RenderFrame(int fb, int width, int height, const float* viewProj, const float* model);
    void Engine_SetLight(const float* lightDir, const float* lightColor, const float* viewPos);
    void Engine_Shutdown();
}