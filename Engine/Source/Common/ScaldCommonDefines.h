#pragma once

#if defined(WIN32) || defined(_WINDOWS)
    #ifdef _EXPORTING
        #define SCALD_API __declspec(dllexport)
    #elif defined(_IMPORTING)
        #define SCALD_API __declspec(dllimport)
    #else
        #define SCALD_API
    #endif
#elif defined(__linux__)
        #define SCALD_API __attribute__((visibility("default")))
#endif

/*
 * Engine CPP wrappers
 */
#ifndef FORCEINLINE
#define FORCEINLINE __forceinline
#endif

#ifndef VVOID
#define VVOID virtual void
#endif