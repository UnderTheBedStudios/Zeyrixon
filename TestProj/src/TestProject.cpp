#include <Zeyrixon.h>

namespace
{
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

Zeyrixon::Application* Zeyrixon::CreateApplication()
{
    return new TestProject();
}