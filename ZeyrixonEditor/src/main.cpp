#include <Zeyrixon.h>

namespace
{
    /* This is for editor applications */
    class ZeyrixonEditor : public Zeyrixon::Application
    {
    public:
        ZeyrixonEditor()
        {
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
    ZeyrixonEditor* editor = new ZeyrixonEditor();

    editor->ChangeWindowImage("Icon.png");
    editor->ChangeWindowTitle("Zeyrixon Editor");

    return editor;
}