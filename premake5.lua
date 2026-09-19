workspace("Zeyrixon")
	architecture("x64")
	configurations({ "Debug", "Release", "Dist" })

	local project_root = path.getabsolute(".")
	project_root = project_root:gsub("\\", "/")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

package.path = package.path .. ";./?.lua"
require "export-compile-commands"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Zeyrixon/vendor/GLFW/include"
IncludeDir["Glad"] = "Zeyrixon/vendor/GLAD/include"
IncludeDir["ImGui"] = "ZeyrixonEditor/vendor/imgui"
IncludeDir["stb"] = "Zeyrixon/vendor/stb"
IncludeDir["TinyFileDialogs"] = "ZeyrixonEditor/vendor/tinyfiledialogs"
IncludeDir["glm"] = "Zeyrixon/vendor/glm"

include "Zeyrixon/vendor/GLFW"
include "Zeyrixon/vendor/GLAD"
include "ZeyrixonEditor/vendor/imgui"

project("Zeyrixon")
	location("Zeyrixon")
	kind("SharedLib")
	language("C++")

	targetdir("bin/" .. outputdir .. "/%{prj.name}")
	objdir("int/" .. outputdir .. "/%{prj.name}")

	pchheader("pch.h")
	pchsource("Zeyrixon/src/pch.cpp")

	files({
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl"
	})

	includedirs({
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.glm}",
	})

	links({
		"GLFW",
		"Glad",
		"ImGui"
	})

	defines { 'Z_PROJECT_ROOT="' .. project_root .. '/"' }

	filter("system:windows")
		links({ "opengl32" })
		defines("Z_PLATFORM_WINDOWS")

	filter("system:linux")
		links({ "GL" })
		defines("Z_PLATFORM_LINUX")

	filter("system:macosx")
		defines("Z_PLATFORM_MAC")
		links({ "OpenGL.framework" })

	filter({})

	filter("system:windows")
    cppdialect("C++17")
    staticruntime("On")
    systemversion("latest")

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY"
	})

	filter("configurations:Debug")
		defines("Z_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		optimize("On")

	filter({ "system:windows", "configurations:Debug" })
		buildoptions("/MDd")

	filter({ "system:windows", "configurations:Release" })
		buildoptions("/MD")

	filter({ "system:windows", "configurations:Dist" })
		buildoptions("/MD")

project("TestProj")
	location("TestProj")
	kind("ConsoleApp")
	language("C++")

	targetdir("bin/" .. outputdir .. "/%{prj.name}")
	objdir("int/" .. outputdir .. "/%{prj.name}")

	files({
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
	})

	includedirs
	{
		"TestProj/src",
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.glm}",
	}

	links({
		"Zeyrixon",
		"ImGui"
	})

	filter("system:windows")
		cppdialect("C++17")
		staticruntime("On")
		systemversion("latest")

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY"
	})

	filter("configurations:Debug")
		defines("Z_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		optimize("On")

	filter({ "system:windows", "configurations:Debug" })
		buildoptions("/MDd")

	filter({ "system:windows", "configurations:Release" })
		buildoptions("/MD")

	filter({ "system:windows", "configurations:Dist" })
		buildoptions("/MD")


project("ZeyrixonEditor")
	location("ZeyrixonEditor")
	kind("ConsoleApp")
	language("C++")

	targetdir("bin/" .. outputdir .. "/%{prj.name}")
	objdir("int/" .. outputdir .. "/%{prj.name}")

	files({
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/tinyfiledialogs/tinyfiledialogs.c",
		"%{prj.name}/vendor/tinyfiledialogs/tinyfiledialogs.h",
	})

	includedirs
	{
		"ZeyrixonEditor/src",
		"Zeyrixon/src",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.TinyFileDialogs}",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.glm}",
	}

	links({
		"Zeyrixon",
		"ImGui",
		"fmt"
	})

	filter("system:windows")
		cppdialect("C++17")
		staticruntime("On")
		systemversion("latest")

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY"
	})

	defines { 'Z_PROJECT_ROOT="' .. project_root .. '/"' }

	filter("configurations:Debug")
		defines("Z_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		optimize("On")

	filter({ "system:windows", "configurations:Debug" })
		buildoptions("/MDd")

	filter({ "system:windows", "configurations:Release" })
		buildoptions("/MD")

	filter({ "system:windows", "configurations:Dist" })
		buildoptions("/MD")