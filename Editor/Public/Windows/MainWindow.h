#pragma once

#include <Editor/Public/Common/Window.h>
#include <Editor/Public/Common/EditorCamera.h>
#include <Editor/Public/Common/Project.h>
#include <glad/glad.h>
#include <imgui.h>

class MainWindow : public Window
{
public:
    explicit MainWindow(const WindowDesc& desc = WindowDesc{});
    ~MainWindow();

    void DrawFrame(int& screenWidth, int& screenHeight);

    void LoadProject(const std::string& zeyrixonPath);
    [[nodiscard]] bool HasProject() const { return m_hasProject; }
    [[nodiscard]] const Project& CurrentProject() const { return m_project; }

private:
    void CreateEntity(const std::string& entityType);

    GLuint sceneViewportFBO = 0, sceneViewportColorTex = 0, sceneViewportDepthRBO = 0;
    int sceneViewportW = 0, sceneViewportH = 0;

    GLuint gameViewportFBO = 0, gameViewportColorTex = 0, gameViewportDepthRBO = 0;
    int gameViewportW = 0, gameViewportH = 0;

    ImVec4 clear_color;
    EditorCamera editorCamera;

    Project m_project;
    bool m_hasProject = false;

    char m_renameWorldBuffer[128] = {};

    int m_selectedEntity = -1;

    // Local editable copy of the selected entity's name — only re-synced from the Engine
    // when the selection changes, so it doesn't overwrite what the user is mid-typing.
    char m_renameBuffer[128] = {};
    int m_renameBufferForEntity = -1;

    // Local editable copy of the selected entity's model path — same re-sync-on-selection-
    // change pattern as m_renameBuffer above.
    char m_modelPathBuffer[256] = {};
    int m_modelPathBufferForEntity = -1;
    std::string m_modelLoadError;

    std::filesystem::path assetRoot;
    std::filesystem::path m_selectedContentFolder = m_project.Directory();
};