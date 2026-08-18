#pragma once

#include <Editor/Public/Common/Window.h>
#include <glad/glad.h>
#include <imgui.h>
#include <string>
#include <filesystem>

struct ProjectEntry
{
    std::string name;
    std::filesystem::path zeyrixonPath;
    std::filesystem::path thumbnailPath;
    GLuint thumbnailTex = 0;
    int thumbW = 0, thumbH = 0;
};

struct ProjectTemplate
{
    std::string name;
    std::string badge;
    std::filesystem::path templateDir;
    std::filesystem::path previewPath;
    GLuint previewTex = 0;
    int previewW = 0, previewH = 0;
};

class ProjectBrowserDialog : public Window
{
public:
    // desc.parent must be set to the owning window's GLFWwindow* handle (for GL sharing + centering).
    // parentWindow is the Window* wrapper itself, used only to check ShouldClose().
    explicit ProjectBrowserDialog(const WindowDesc& desc, Window* parentWindow = nullptr);
    ~ProjectBrowserDialog();

    void DrawFrame();

    // True once the dialog has a result (selection made, or cancelled) and is ready to be destroyed.
    bool IsFinished() const { return m_finished; }
    bool HasSelectedProject() const { return m_hasSelectedProject; }
    const std::string& SelectedProjectPath() const { return m_selectedProjectPath; }

    void ScanProjects();
    void LoadTemplates();

private:
    std::vector<ProjectEntry> m_recentProjects;
    bool m_scanned = false;
    int m_selectedIndex = -1;

    std::vector<ProjectTemplate> m_templates;
    bool m_templatesLoaded = false;
    int m_selectedTemplateIndex = 0;

    Window* m_parentWindow = nullptr;

    bool m_hasSelectedProject = false;
    bool m_finished = false;
    std::string m_selectedProjectPath;

    ImVec4 clear_color;
};