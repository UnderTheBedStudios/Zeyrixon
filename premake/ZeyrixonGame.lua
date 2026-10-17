-- Shared build settings for every game made with Zeyrixon.
--
-- A game's premake5.lua sets ZEYRIXON_DIR (the folder that holds the engine's premake5.lua)
-- and then includes this file. After that, calling ZeyrixonLink() inside a `project` block
-- gives that project everything it needs to use the engine:
--
--   * the engine's headers (and the ones those headers include: glm, entt, spdlog, AngelScript...)
--   * the engine's static libraries, in a link order that works with GCC/Clang
--   * the preprocessor defines the engine expects
--   * the system libraries OpenGL needs on each platform
--
-- Keeping this in the engine, not in each game, means that when the engine gains a new
-- dependency (as it did with AngelScript) every game picks it up without being edited.

assert(ZEYRIXON_DIR, "ZeyrixonGame.lua: set ZEYRIXON_DIR before including this file")
ZEYRIXON_DIR = (ZEYRIXON_DIR:gsub("\\", "/"))

-- Must be the same string the engine's own premake5.lua uses: it is how we find its libraries.
ZeyrixonOutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

local function engine(path)
	return ZEYRIXON_DIR .. "/" .. path
end

-- Each vendor project (GLFW, Glad, ImGui, AngelScript) writes its library next to its own
-- premake5.lua, under bin/<config>/<name>. The engine itself writes to the root bin/.
local function libFolder(projectFolder, name)
	return engine(projectFolder .. "/bin/" .. ZeyrixonOutputDir .. "/" .. name)
end

function ZeyrixonLink()
	includedirs
	{
		engine("Zeyrixon/src"),
		engine("Zeyrixon/vendor/spdlog/include"),
		engine("Zeyrixon/vendor/GLFW/include"),
		engine("Zeyrixon/vendor/GLAD/include"),
		engine("Zeyrixon/vendor/stb"),
		engine("Zeyrixon/vendor/glm"),
		engine("Zeyrixon/vendor/entt/include"),
		engine("Zeyrixon/vendor/pugixml/src"),
		engine("Zeyrixon/vendor/AngelScript/include"),
		engine("Zeyrixon/vendor/AngelScript/add_on"),
		engine("ZeyrixonEditor/vendor/imgui"),
	}

	libdirs
	{
		engine("bin/" .. ZeyrixonOutputDir .. "/Zeyrixon"),
		libFolder("Zeyrixon/vendor/AngelScript", "AngelScript"),
		libFolder("ZeyrixonEditor/vendor/imgui", "ImGui"),
		libFolder("Zeyrixon/vendor/GLFW", "GLFW"),
		libFolder("Zeyrixon/vendor/GLAD", "Glad"),
	}

	-- Order matters for static libraries on Linux: a library must come BEFORE the libraries
	-- it uses. Zeyrixon uses all the others, and ImGui uses GLFW.
	links { "Zeyrixon", "ImGui", "GLFW", "Glad", "AngelScript" }

	defines
	{
		"BUILD_DLL",
		"GLFW_INCLUDE_NONE",
		"FMT_HEADER_ONLY",
		'Z_PROJECT_ROOT="' .. ZEYRIXON_DIR .. '/"',
	}

	filter "system:linux"
		links { "GL", "dl", "pthread", "X11" }

	filter "system:windows"
		systemversion "latest"
		links { "opengl32", "gdi32" }
		defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN" }

	filter { "system:windows", "toolset:gcc" }
		linkoptions { "-static", "-Wl,--whole-archive", "-lwinpthread", "-Wl,--no-whole-archive" }

	filter "action:vs*"
		buildoptions { "/utf-8" }

	filter "configurations:Debug"
		defines "Z_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "Z_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "Z_DIST"
		runtime "Release"
		optimize "On"

	-- Always leave the filter cleared: a filter left active inside this function would
	-- silently apply to whatever the game's own premake5.lua writes after calling it.
	filter {}
end
