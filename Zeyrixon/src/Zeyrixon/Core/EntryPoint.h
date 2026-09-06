#pragma once

#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/Core/Log.h>

extern Zeyrixon::Application* Zeyrixon::CreateApplication();

int main(int argc, char** argv)
{
    Zeyrixon::Log::Init();

    Z_CORE_WARN("Initialized Log!");

    auto app = Zeyrixon::CreateApplication();
    app->Run();
    delete app;

    return 0;
}