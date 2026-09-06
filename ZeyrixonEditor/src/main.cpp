#include <Zeyrixon.h>
#include <ZeyrixonEditor/Core/EditorLayer.h>

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
    Z_CORE_TRACE("Editor");
    Editor::ZeyrixonEditor* editor = new Editor::ZeyrixonEditor();

    editor->ChangeWindowImage("Icon.png");
    editor->ChangeWindowTitle("Zeyrixon Editor");

    return editor;
}