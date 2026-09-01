workspace("Zeyrixon")
	architecture("x64")
	configurations({ "Debug", "Release", "Dist" })

	local project_root = path.getabsolute(".")

	project_root = project_root:gsub("\\", "/")
	project_root = project_root:gsub(" ", "\\ ")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

package.path = package.path .. ";./?.lua"
require "export-compile-commands"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Zeyrixon/vendor/GLFW/include"
IncludeDir["Glad"] = "Zeyrixon/vendor/GLAD/include"
IncludeDir["stb"] = "Zeyrixon/vendor/stb"

include("Zeyrixon/vendor/GLFW")
include("Zeyrixon/vendor/GLAD")

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
	})

	includedirs({
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.stb}",
	})

	links({
		"GLFW",
		"Glad",
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
		"GLFW_INCLUDE_NONE"
	})

	postbuildcommands({
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/TestProj" },
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/ZeyrixonEditor" },
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

	includedirs({
		"TestProj/src",
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
	})

	links({
		"Zeyrixon",
	})

	filter("system:windows")
		cppdialect("C++17")
		staticruntime("On")
		systemversion("latest")

	defines({
		"BUILD_DLL",
	})

	postbuildcommands({
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/TestProj" },
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/ZeyrixonEditor" },
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
	})

	includedirs({
		"ZeyrixonEditor/src",
		"Zeyrixon/src",
		"Zeyrixon/vendor/spdlog/include",
	})

	links({
		"Zeyrixon",
	})

	filter("system:windows")
		cppdialect("C++17")
		staticruntime("On")
		systemversion("latest")

	defines({
		"BUILD_DLL",
	})

	postbuildcommands({
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/TestProj" },
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/ZeyrixonEditor" },
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