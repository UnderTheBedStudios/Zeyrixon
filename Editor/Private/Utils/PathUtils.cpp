#include <Editor/Public/Utils/PathUtils.h>
#include <stdexcept>

#if defined(__linux__)
#include <unistd.h>
#include <climits>
#include <sys/types.h>
#include <pwd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace PathUtils
{

fs::path GetExecutablePath()
{
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1)
        throw std::runtime_error("Failed to resolve /proc/self/exe");
    buf[len] = '\0';
    return fs::path(buf);
#elif defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        throw std::runtime_error("Failed to resolve executable path");
    return fs::path(buf);
#else
    #error "Unsupported platform"
#endif
}


fs::path GetDocumentsDirectory()
{
#if defined(__linux)
    const char* home_env = std::getenv("HOME");
    fs::path home_path;

    if (home_env != nullptr) {
        home_path = fs::path(home_env);
    } else {
        // Fallback if $HOME variable is not set (e.g., cron jobs or specific system daemons)
        struct passwd* pw = getpwuid(getuid());
        if (pw != nullptr && pw->pw_dir != nullptr) {
            home_path = fs::path(pw->pw_dir);
        }
    }

    if (!home_path.empty()) {
        fs::path docs_path = home_path / "Documents";
        if (fs::exists(docs_path)) {
            return docs_path;
        }
        return home_path; // Absolute fallback to home root if Documents directory is deleted
    }
    return fs::path();
#elif defined(_WIN32)
    PWSTR uuid_path = nullptr;
    // FOLDERID_Documents ensures correctness even if the user relocated their Documents folder
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &uuid_path);
    
    if (SUCCEEDED(hr) && uuid_path != nullptr) {
        fs::path docs_path(uuid_path);
        CoTaskMemFree(uuid_path); // Free memory allocated by the COM API
        return docs_path;
    }
    
    // Fallback if the Win32 API fails
    const char* user_profile = std::getenv("USERPROFILE");
    if (user_profile != nullptr) {
        return fs::path(user_profile) / "Documents";
    }
    return fs::path();
#else
    #error "Unsupported platform
    return fs::path();
#endif
}

fs::path ResolveProjectRoot(const std::string& markerFilename)
{
    fs::path dir = GetExecutablePath().parent_path();

    while (true)
    {
        if (fs::exists(dir / markerFilename))
            return dir;

        fs::path parent = dir.parent_path();
        if (parent == dir)
            throw std::runtime_error("Could not locate project root (marker file '" + markerFilename + "' not found)");

        dir = parent;
    }
}

} // namespace PathUtils