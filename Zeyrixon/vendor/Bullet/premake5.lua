-- Bullet Physics, built as ONE static library from the three folders the engine needs.
-- The Bullet sources live in the `bullet3` submodule next to this file (the same layout as
-- vendor/AngelScript: a premake wrapper beside a clone of the upstream repository).
--
--   LinearMath       vectors, matrices, allocators
--   BulletCollision  shapes, broadphase, narrowphase
--   BulletDynamics   rigid bodies, solver, constraints
--
-- We skip Bullet3*, BulletSoftBody, BulletInverseDynamics and Extras: the engine doesn't use them.
-- One library instead of three also means there is no link ORDER to get wrong on Linux.

project "Bullet"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "Off"
    warnings "Off"
    pic "On"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "bullet3/src/LinearMath/**.h",
        "bullet3/src/LinearMath/**.cpp",
        "bullet3/src/BulletCollision/**.h",
        "bullet3/src/BulletCollision/**.cpp",
        "bullet3/src/BulletDynamics/**.h",
        "bullet3/src/BulletDynamics/**.cpp",
        "bullet3/src/btBulletCollisionCommon.h",
        "bullet3/src/btBulletDynamicsCommon.h",
    }

    includedirs { "bullet3/src" }

    filter "system:linux"
        links { "pthread" }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }

    filter "toolset:gcc or toolset:clang"
        buildoptions { "-fno-strict-aliasing" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter "configurations:Dist"
        runtime "Release"
        optimize "On"
        symbols "Off"

    filter {}
