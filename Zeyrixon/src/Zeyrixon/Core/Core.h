#pragma once

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

/* This is just because Intellisense is a jerk and doesn't like the Z_PROJECT_ROOT :( */
#ifdef __INTELLISENSE__
    #define Z_STR_IMPL(x) #x
    #define Z_STR(x) Z_STR_IMPL(x)
    #define Z_ROOT_PATH Z_STR(Z_PROJECT_ROOT)
#else
    #define Z_ROOT_PATH Z_PROJECT_ROOT
#endif