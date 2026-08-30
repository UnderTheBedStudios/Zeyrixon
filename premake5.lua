workspace("Zeyrixon")
	architecture("x64")
	configurations({ "Debug", "Release", "Dist" })

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Zeyrixon/vendor/GLFW/include"

include("Zeyrixon/vendor/GLFW")

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
	})

	links({
		"GLFW",
	})

	filter("system:windows")
		links({ "opengl32" })

	filter("system:linux")
		links({ "GL" })

	filter("system:macosx")
		links({ "OpenGL.framework" })

	filter({})

	filter("system:windows")
		cppdialect("C++17")
		staticruntime("On")
		systemversion("latest")

	defines({
		"BUILD_DLL",
	})

	postbuildcommands({
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox" },
	})

	filter("configurations:Debug")
		defines("HZ_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("HZ_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("HZ_DIST")
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
	})

	filter("configurations:Debug")
		defines("HZ_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("HZ_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("HZ_DIST")
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
		{ "{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox" },
	})

	filter("configurations:Debug")
		defines("HZ_DEBUG")
		symbols("On")

	filter("configurations:Release")
		defines("HZ_RELEASE")
		optimize("On")

	filter("configurations:Dist")
		defines("HZ_DIST")
		optimize("On")

	filter({ "system:windows", "configurations:Debug" })
		buildoptions("/MDd")

	filter({ "system:windows", "configurations:Release" })
		buildoptions("/MD")

	filter({ "system:windows", "configurations:Dist" })
		buildoptions("/MD")
