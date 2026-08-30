#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifdef BUILD_DLL
        #define Z_API __declspec(dllexport)
    #else
        #define Z_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define Z_API __attribute__((visibility("default")))
#else
    #define Z_LAPI
#endif

#define BIT(x) (1 << x)