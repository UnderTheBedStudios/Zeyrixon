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
IncludeDir["cgltf"] = "Zeyrixon/vendor/cgltf"
IncludeDir["TinyFileDialogs"] = "ZeyrixonEditor/vendor/tinyfiledialogs"
IncludeDir["glm"] = "Zeyrixon/vendor/glm"
IncludeDir["pugixml"] = "Zeyrixon/vendor/pugixml/src"
IncludeDir["entt"] = "Zeyrixon/vendor/entt/include"
IncludeDir["ImGuizmo"] = "ZeyrixonEditor/vendor/ImGizmo/ImGuizmo/src"
IncludeDir["AngelScript"] = "Zeyrixon/vendor/AngelScript/sdk/angelscript/include"
IncludeDir["AngelScriptAddOn"] = "Zeyrixon/vendor/AngelScript/sdk/add_on"

include "Zeyrixon/vendor/GLFW"
include "Zeyrixon/vendor/GLAD"
include "ZeyrixonEditor/vendor/imgui"
include "Zeyrixon/vendor/AngelScript"

project("Zeyrixon")
	location("Zeyrixon")
	kind("StaticLib")
	language("C++")
	cppdialect "C++20"
	multiprocessorcompile("on")
	staticruntime "Off"

	filter("action:vs*")
		buildoptions({ "/utf-8" })
	filter({})

	targetdir("bin/" .. outputdir .. "/%{prj.name}")
	objdir("int/" .. outputdir .. "/%{prj.name}")

	pchheader("pch.h")
	pchsource("Zeyrixon/src/pch.cpp")

	files({
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",
		"%{prj.name}/vendor/pugixml/src/**.cpp",
		"%{prj.name}/vendor/pugixml/src/*.hpp",
	})

	filter("files:**/pugixml.cpp")
		enablepch("Off")
	filter({})

	includedirs({
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.cgltf}",
		"%{IncludeDir.glm}",
		"%{IncludeDir.pugixml}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.AngelScript}",
		"%{IncludeDir.AngelScriptAddOn}",
	})

	links({
		"GLFW",
		"Glad",
		"ImGui",
		"AngelScript",
	})

	filter("system:windows")
		links({ "opengl32" })
		defines({ "Z_PLATFORM_WINDOWS", "NOMINMAX", "WIN32_LEAN_AND_MEAN" })

	filter("system:linux")
		links({ "GL" })
		defines("Z_PLATFORM_LINUX")

	filter("system:macosx")
		defines("Z_PLATFORM_MAC")
		links({ "OpenGL.framework" })

	filter({})

	filter("system:windows")
    	systemversion("latest")

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY",
		'Z_PROJECT_ROOT="' .. project_root .. '/"'
	})

	filter("configurations:Debug")
		defines("Z_DEBUG")
		runtime("Debug")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		runtime("Release")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		runtime("Release")
		optimize("On")

project("TestProj")
	location("TestProj")
	kind("ConsoleApp")
	language("C++")
	cppdialect("C++20")
	multiprocessorcompile("on")

	filter("action:vs*")
		buildoptions({ "/utf-8" })

	filter({})

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
		"%{IncludeDir.entt}",
		"%{IncludeDir.AngelScript}",
		"%{IncludeDir.AngelScriptAddOn}",
		"%{IncludeDir.pugixml}",
	}

	links({
		"Zeyrixon",
		"ImGui",
		"GLFW",
		"Glad",
		"AngelScript",
	})

	filter("system:windows")
		systemversion("latest")
		links({ "opengl32", "gdi32" })
		defines({ "NOMINMAX", "WIN32_LEAN_AND_MEAN" })

	filter({ "system:windows", "toolset:gcc" })
		linkoptions({ "-static", "-Wl,--whole-archive", "-lwinpthread", "-Wl,--no-whole-archive" })

	filter({})

	filter("system:linux")
		links({ "GL", "dl", "pthread", "X11" })

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY",
		'Z_PROJECT_ROOT="' .. project_root .. '/"'
	})

	filter("configurations:Debug")
		defines("Z_DEBUG")
		runtime("Debug")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		runtime("Release")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		runtime("Release")
		optimize("On")

project("ZeyrixonEditor")
	location("ZeyrixonEditor")
	kind("ConsoleApp")
	language("C++")
	cppdialect("C++20")
	multiprocessorcompile("on")

	filter("action:vs*")
		buildoptions({ "/utf-8" })
	filter({})

	targetdir("bin/" .. outputdir .. "/%{prj.name}")
	objdir("int/" .. outputdir .. "/%{prj.name}")

	files({
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/ImGizmo/ImGuizmo/src/**.h",
		"%{prj.name}/vendor/ImGizmo/ImGuizmo/src/**.cpp",
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
		"%{IncludeDir.entt}",
		"%{IncludeDir.AngelScript}",
		"%{IncludeDir.AngelScriptAddOn}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.pugixml}",
	}

	links({
		"Zeyrixon",
		"ImGui",
		"GLFW",
		"Glad",
		"AngelScript",
	})

	filter("system:windows")
		systemversion("latest")
		links({ "opengl32", "gdi32", "comdlg32", "ole32" })
		defines({ "NOMINMAX" })

	filter({ "system:windows", "toolset:gcc" })
		linkoptions({ "-static", "-Wl,--whole-archive", "-lwinpthread", "-Wl,--no-whole-archive" })

	filter({})

	filter("system:linux")
		links({ "GL", "dl", "pthread", "X11" })

	filter({})

	defines({
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY",
		'Z_PROJECT_ROOT="' .. project_root .. '/"'
	})

	filter("configurations:Debug")
		defines("Z_DEBUG")
		runtime("Debug")
		symbols("On")

	filter("configurations:Release")
		defines("Z_RELEASE")
		runtime("Release")
		optimize("On")

	filter("configurations:Dist")
		defines("Z_DIST")
		runtime("Release")
		optimize("On")
