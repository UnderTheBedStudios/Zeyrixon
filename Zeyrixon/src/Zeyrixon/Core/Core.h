#pragma once

#include <string>

namespace Zeyrixon { const std::string& GetRootPath(); }

/* All ts is is setting up the API for later dll imports/exports, please don't look bro T_T */
#if defined(_WIN32) || defined(_WIN64)
    #ifdef BUILD_DLL
        /* All ts is is setting up the API for later dll imports/exports, please don't look bro T_T */
        #define Z_API __declspec(dllexport)
    #else
        /* All ts is is setting up the API for later dll imports/exports, please don't look bro T_T */
        #define Z_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    /* All ts is is setting up the API for later dll imports/exports, please don't look bro T_T */
    #define Z_API __attribute__((visibility("default")))
#else
    /* All ts is is setting up the API for later dll imports/exports, please don't look bro T_T */
    #define Z_API
#endif

/* Just makes bits exist */
#define BIT(x) (1 << x)

/* Root folder of the Zeyrixon install/checkout (always ends in '/').
   Resolved at runtime (see Paths.cpp) instead of being baked in at compile time, so a binary built on one
   machine (e.g. cross-compiled on Linux) still finds its assets on another. Yields a const char*. */
#define Z_ROOT_PATH (::Zeyrixon::GetRootPath().c_str())
