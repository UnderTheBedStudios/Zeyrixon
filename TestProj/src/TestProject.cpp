#include <Zeyrixon.h>

namespace
{
    /* It inherits from the Application so that it can have the same functions and yah */
    class TestProject : public Zeyrixon::Application
    {
    public:
        TestProject()
        {
        }

        ~TestProject()
        {

        }
    };
}

/* This just makes it so that your app exists :) */
Zeyrixon::Application* Zeyrixon::CreateApplication()
{
    TestProject* project = new TestProject();
    project->ChangeWindowImage("Icon.png");
    return project;
}