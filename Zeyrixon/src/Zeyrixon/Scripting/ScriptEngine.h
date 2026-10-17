#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/DeltaTime.h>

#include <string>

/* Forward declared so that including this header does not drag <angelscript.h> into every
   file. A game that wants to register its own functions includes <angelscript.h> itself. */
class asIScriptEngine;

namespace Zeyrixon
{
    class World;

    /* The AngelScript side of the engine. Everything is static because there is exactly one
       scripting engine per process.

       Lifetime:   Init() / Shutdown()           called by Application
       Per play:   OnRuntimeStart / OnRuntimeUpdate / OnRuntimeStop, called by World */
    class Z_API ScriptEngine
    {
    public:
        static void Init();
        static void Shutdown();

        /* The raw AngelScript engine, so a game can register its own native types and
           functions (call this before the world starts). Null before Init(). */
        static asIScriptEngine* GetEngine();

        /* Relative ScriptComponent paths are resolved against this folder. The editor and the
           RuntimeLayer set it to the project's Assets folder when a project is loaded. */
        static void SetAssetRoot(const std::string& directory);
        static const std::string& GetAssetRoot();

        /* Compiles every script used by an entity in the world, creates one script object per
           ScriptComponent and calls OnCreate on each. Returns false (and runs nothing) if a
           script fails to compile. */
        static bool OnRuntimeStart(World& world);
        static void OnRuntimeUpdate(World& world, DeltaTime dt);
        static void OnRuntimeStop(World& world);

        static bool IsRunning();
    };
}
