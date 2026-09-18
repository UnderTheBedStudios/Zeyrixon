#include <Zeyrixon.h>

class TestLayer : public Zeyrixon::Layer
{
public:
    TestLayer()
        : Layer("Test")
    {
    }

    void OnUpdate() override
    {
        if (Zeyrixon::Input::IsKeyPressed(Z_KEY_TAB))
            Z_INFO("Tab key was pressed!");

        if (Zeyrixon::Input::IsMouseButtonPressed(Z_MOUSE_BUTTON_LEFT))
            Z_INFO("Left Mouse Button was pressed!");
    }
};

namespace
{
    /* It inherits from the Application so that it can have the same functions and yah */
    class TestProject : public Zeyrixon::Application
    {
    public:
        TestProject()
        {
            PushLayer(new TestLayer());
        }

        ~TestProject()
        {

        }
    };
}

/* This just makes it so that your app exists :) */
Zeyrixon::Application* Zeyrixon::CreateApplication(Zeyrixon::ApplicationCommandLineArgs args)
{
    TestProject* project = new TestProject();
    project->ChangeWindowImage("Icon.png");
    project->ChangeWindowTitle("Test Project");
    return project;
}