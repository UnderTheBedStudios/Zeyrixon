#include <pch.h>
#include <Zeyrixon/Core/Core.h>

#include <filesystem>
#include <cstdlib>

#if defined(_WIN32) || defined(_WIN64)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#ifdef __INTELLISENSE__
    #define Z_PROJECT_ROOT_STR ""
#else
    #define Z_PROJECT_ROOT_STR Z_PROJECT_ROOT
#endif

namespace Zeyrixon {

    namespace fs = std::filesystem;

    namespace {

        /* A folder only counts as the Zeyrixon root (a source checkout) if it contains this. The editor's own
           resources (font, icons) are compiled in, so only source-tree features (engine browser, new-project
           build files) look at the root at all. */
        const fs::path kRootMarker = fs::path("Zeyrixon") / "src";

        bool IsRoot(const fs::path& dir)
        {
            if (dir.empty())
                return false;
            std::error_code ec;
            return fs::is_directory(dir / kRootMarker, ec);
        }

        /* Walks up from `start` looking for a folder that IsRoot(). */
        fs::path WalkUp(fs::path start)
        {
            std::error_code ec;
            while (!start.empty())
            {
                if (IsRoot(start))
                    return start;
                fs::path parent = start.parent_path();
                if (parent == start)
                    break;
                start = parent;
            }
            return {};
        }

        fs::path ExecutableDirectory()
        {
#if defined(_WIN32) || defined(_WIN64)
            wchar_t buffer[MAX_PATH * 4];
            DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
            if (length == 0 || length >= std::size(buffer))
                return {};
            return fs::path(buffer).parent_path();
#else
            std::error_code ec;
            fs::path exe = fs::read_symlink("/proc/self/exe", ec);
            return ec ? fs::path() : exe.parent_path();
#endif
        }

        std::string Normalise(const fs::path& p)
        {
            std::string s = p.generic_string();
            if (!s.empty() && s.back() != '/')
                s += '/';
            return s;
        }

        std::string ResolveRoot()
        {
            // 1. Explicit override.
            if (const char* env = std::getenv("ZEYRIXON_ROOT"))
            {
                if (IsRoot(env))
                    return Normalise(env);
            }

            const fs::path exeDir = ExecutableDirectory();

#ifndef Z_DIST
            // 2. Development: the source checkout this binary was built from. Dist builds skip this on
            //    purpose, so a Dist build on your own machine behaves like it will on everyone else's
            //   .
            const fs::path compiled = Z_PROJECT_ROOT_STR;
            if (IsRoot(compiled))
                return Normalise(compiled);

            // 3. Development: exe or working directory somewhere inside a checkout.
            if (fs::path found = WalkUp(exeDir); !found.empty())
                return Normalise(found);
            std::error_code ec;
            if (fs::path found = WalkUp(fs::current_path(ec)); !found.empty())
                return Normalise(found);
#endif

            // 4. Nothing found (the normal case for a standalone build). Use the exe folder; callers must cope with missing files.
            return Normalise(exeDir);
        }
    }

    const std::string& GetRootPath()
    {
        static const std::string root = ResolveRoot();
        return root;
    }
}
