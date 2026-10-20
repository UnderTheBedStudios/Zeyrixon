#include <Zeyrixon.h>
#include <ZeyrixonEditor/Core/EditorLayer.h>

#ifdef __linux__
#include <cstdlib>
#include <string>
#include <fstream>
#include <filesystem>
#include <unistd.h>

// Embedded copy of tools/dialog-shim/which so Dist builds don't depend on the source tree.
static const char* kDialogShimScript = R"SHIM(#!/bin/sh
# Zeyrixon dialog-backend shim.
# Makes tinyfiledialogs' "is zenity installed?" check report "no" when
# ZEYRIXON_PREFER_KDIALOG is set, so it falls through to kdialog instead.

if [ "$1" = "zenity" ] && [ -n "$ZEYRIXON_PREFER_KDIALOG" ]; then
    exit 1
fi

# Delegate to the real "which", skipping our own shim directory.
PATH=$(echo "$PATH" | tr ':' '\n' | grep -vx "$(dirname "$0")" | tr '\n' ':')
exec which "$@"
)SHIM";

// Writes the shim into a per-user dir (not a shared /tmp path, which another user could pre-create
// and use to hijack PATH). Returns "" on failure.
static std::string EnsureDialogShimDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path base;
    if (const char* x = getenv("XDG_RUNTIME_DIR"); x && *x)       base = x;
    else if (const char* c = getenv("XDG_CACHE_HOME"); c && *c)   base = c;
    else if (const char* h = getenv("HOME"); h && *h)             base = fs::path(h) / ".cache";
    else return {};

    fs::path dir = base / "zeyrixon" / "dialog-shim";
    fs::create_directories(dir, ec);
    if (ec) return {};

    // Write to a temp name then rename, so a second editor starting at the same time never sees a half-written file.
    fs::path tmp  = dir / ("which.tmp." + std::to_string(getpid()));
    fs::path dest = dir / "which";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return {};
        out << kDialogShimScript;
    }
    using P = fs::perms;
    fs::permissions(tmp, P::owner_all | P::group_read | P::group_exec | P::others_read | P::others_exec, ec);
    fs::rename(tmp, dest, ec);
    if (ec) { fs::remove(tmp, ec); return {}; }

    return dir.string();
}

static void PrependDialogShimToPath()
{
    const std::string shimDir = EnsureDialogShimDir();
    if (shimDir.empty())
        return;

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
            UseDefaultWindowImage();
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
