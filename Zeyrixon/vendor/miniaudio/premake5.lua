project "miniaudio"
    kind "StaticLib"
    language "C"
    pic "On"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "miniaudio/miniaudio.h",
        "miniaudio_impl.c"
    }

    includedirs { "miniaudio" }

    filter "system:windows"
        systemversion "latest"
        staticruntime "Off"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        runtime "Release"
        optimize "On"

    filter "configurations:Dist"
        runtime "Release"
        optimize "On"