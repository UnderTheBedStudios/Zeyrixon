#include <pch.h>
#include <Zeyrixon/Core/RuntimeLayer.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>

#include <glad/glad.h>

namespace Zeyrixon
{
    RuntimeLayer::RuntimeLayer(const std::string& projectManifestPath)
        : Layer("RuntimeLayer"), m_ManifestPath(projectManifestPath)
    {
    }

    void RuntimeLayer::OnAttach()
    {
        m_Project = Project::Load(m_ManifestPath);
        if (!m_Project)
        {
            Z_CORE_CRITICAL("RuntimeLayer: could not load project '{0}'", m_ManifestPath);
            return;
        }

        Application::Get().ChangeWindowTitle(m_Project->GetName().c_str());
        ScriptEngine::SetAssetRoot(m_Project->GetAssetDirectory());

        m_World = World::Load(m_Project->GetStartupWorldPath());
        if (!m_World)
        {
            Z_CORE_CRITICAL("RuntimeLayer: could not load the startup world of '{0}'", m_Project->GetName());
            return;
        }

        if (!m_World->OnRuntimeStart())
            Z_CORE_ERROR("RuntimeLayer: the world did not start (script errors?). Drawing it without scripts.");
    }

    void RuntimeLayer::OnDetach()
    {
        if (m_World)
            m_World->OnRuntimeStop();
    }

    void RuntimeLayer::OnUpdate(DeltaTime dt)
    {
        if (!m_World)
            return;

        m_World->OnUpdate(dt);

        Entity camera = m_World->GetPrimaryCamera();
        if (!camera)
            return;

        const auto& transform = camera.GetComponent<TransformComponent>();
        const auto& cameraData = camera.GetComponent<CameraComponent>();

        auto window = Application::Get().GetWindow();
        const float aspect = window->GetHeight() > 0 ? (float)window->GetWidth() / (float)window->GetHeight() : 1.f;

        const glm::mat4 view = transform.GetViewMatrix();
        const glm::mat4 projection = cameraData.GetProjection(aspect);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        WorldRenderer::RenderWorld(*m_World, view, projection, transform.Position);
        WorldRenderer::RenderSprites(*m_World, view, projection);
    }
}
