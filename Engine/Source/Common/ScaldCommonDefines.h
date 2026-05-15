#pragma once

#if defined(WIN32) || defined(_WIN32)
    #ifdef _EXPORTING
        #define SCALD_API __declspec(dllexport)
    #elif defined(_IMPORTING)
        #define SCALD_API __declspec(dllimport)
    #else
        #define SCALD_API
    #endif
#else
    #ifdef _EXPORTING
        #define SCALD_API __attribute__((visibility("default")))
    #elif _IMPORTING
        #define SCALD_API __attribute__((visibility("default")))
    #else
        #define SCALD_API
    #endif
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