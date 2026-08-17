// Engine.cpp : Defines the functions for the static library.

#include <Engine/Public/Engine.h>
#include <Engine/pch.h>
#include <Engine/framework.h>
#include <Engine/Public/General/Shader.h>
#include <Engine/Public/Entities/Common/BaseEntity.h>
#include <Engine/Public/Components/Common/Model.h>
#include <glad/glad.h>
#include <cstdio>
#include <chrono>
#include <math.h>
#include <unistd.h>
#include <string>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>
#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

typedef void* (*GLADloadproc)(const char* name);

namespace {

std::unique_ptr<Shader> g_MainShader;
std::unique_ptr<Shader> g_DepthShader;
std::unique_ptr<Model> g_TestModel;

glm::vec3 g_LightDir = glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
glm::vec3 g_LightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 g_ViewPos    = glm::vec3(0.0f);

std::string g_AssetRoot;

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
    g_TestModel.reset();
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

    // TODO: temporary - swap for real scene/entity loading once that pipeline exists
    g_TestModel = std::make_unique<Model>();
    std::string testModelPath = g_AssetRoot + "/Engine/Models/Camera/Camera.obj";
    if (!g_TestModel->LoadFromFile(testModelPath))
        fprintf(stderr, "[Engine] Failed to load test model from %s\n", testModelPath.c_str());
}

void Engine_RenderFrame(int fb, int width, int height, const float* viewProj, const float* model)
{
    // --- Pass 1: render depth from the light's POV ---
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, g_ShadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    g_DepthShader->use();
    g_DepthShader->setMat4("uLightSpaceMatrix", g_LightSpaceMatrix);
    g_DepthShader->setMat4("uModel", glm::make_mat4(model));

    if (g_TestModel)
        g_TestModel->Draw();

    glDisable(GL_CULL_FACE);

    // --- Pass 2: normal scene render, sampling the shadow map ---
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, width, height);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    g_MainShader->use();
    g_MainShader->setMat4("uViewProj", glm::make_mat4(viewProj));
    g_MainShader->setMat4("uModel", glm::make_mat4(model));
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

    if (g_TestModel)
        g_TestModel->Draw();
}

void Engine_SetLight(const float* lightDir, const float* lightColor, const float* viewPos)
{
    g_LightDir   = glm::normalize(glm::vec3(lightDir[0], lightDir[1], lightDir[2]));
    g_LightColor = glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
    g_ViewPos    = glm::vec3(viewPos[0], viewPos[1], viewPos[2]);
    UpdateLightSpaceMatrix();
}

} // extern "C"

// TODO: This is an example of a library function
void fnEngine()
{
    
}