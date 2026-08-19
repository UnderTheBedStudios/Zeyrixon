#include <filesystem>
#include <climits>

#if defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace fs = std::filesystem;

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