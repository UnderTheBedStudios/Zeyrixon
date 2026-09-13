#include <Zeyrixon.h>
#include <ZeyrixonEditor/Core/EditorLayer.h>

#ifdef __linux__
#include <cstdlib>
#include <string>

static void PrependDialogShimToPath()
{
    std::string shimDir = std::string(Z_PROJECT_ROOT) + "ZeyrixonEditor/tools/dialog-shim";
    const char* existingPath = getenv("PATH");
    std::string newPath = shimDir + ":" + (existingPath ? existingPath : "");
    setenv("PATH", newPath.c_str(), 1);
}
#endif

namespace Editor
{
    /* This is for editor applications */
    class ZeyrixonEditor : public Zeyrixon::Application
    {
    public:
        ZeyrixonEditor()
        {
            PushLayer(new EditorLayer());
        }

        ~ZeyrixonEditor()
        {

        }
    };
}

/* This just makes it so that the Editor app exists :) */
Zeyrixon::Application* Zeyrixon::CreateApplication()
{
#ifdef __linux__
    setenv("ZEYRIXON_PREFER_KDIALOG", "1", 1);
    PrependDialogShimToPath();
#endif
    Z_CORE_TRACE("Editor");
    Editor::ZeyrixonEditor* editor = new Editor::ZeyrixonEditor();

    editor->ChangeWindowImage("Icon.png");
    editor->ChangeWindowTitle("Zeyrixon Editor");

    return editor;
}