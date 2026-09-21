#include <Zeyrixon.h>
#include <ZeyrixonEditor/Core/EditorLayer.h>

#ifdef __linux__
#include <cstdlib>
#include <string>

static void PrependDialogShimToPath()
{
    std::string shimDir = std::string(Z_ROOT_PATH) + "ZeyrixonEditor/tools/dialog-shim";
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
        ZeyrixonEditor(const Zeyrixon::ApplicationCommandLineArgs& args)
            : Zeyrixon::Application(args)
        {
            // Set the defaults first: PushLayer() below runs EditorLayer::OnAttach()
            // immediately (see Application::PushLayer), which will load a startup
            // project and retitle the window - if we set the title after PushLayer
            // instead, we'd clobber that with the generic one.
            ChangeWindowImage("Icon.png");
            ChangeWindowTitle("Zeyrixon Editor");

            // argv[0] is the executable path; argv[1], if present, is the file the
            // OS asked us to open (e.g. double-clicking a .zeyrixon file, or a
            // "kdialog"/"xdg-open" style launch that forwards a path as %f).
            std::string startupProjectPath;
            if (args.Count > 1)
                startupProjectPath = args[1];

            PushLayer(new EditorLayer(startupProjectPath));


        }

        ~ZeyrixonEditor()
        {

        }
    };
}

/* This just makes it so that the Editor app exists :) */
Zeyrixon::Application* Zeyrixon::CreateApplication(Zeyrixon::ApplicationCommandLineArgs args)
{
#ifdef __linux__
    setenv("ZEYRIXON_PREFER_KDIALOG", "1", 1);
    PrependDialogShimToPath();
#endif
    Z_EDITOR_TRACE("Editor");
    Editor::ZeyrixonEditor* editor = new Editor::ZeyrixonEditor(args);

    return editor;
}