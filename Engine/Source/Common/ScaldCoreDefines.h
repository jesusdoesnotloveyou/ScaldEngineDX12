#pragma once

#ifdef _EXPORTING
#define SCALD_API __declspec(dllexport)
#elif _IMPORTING
#define SCALD_API __declspec(dllimport)
#else
#define SCALD_API
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

/*
 * Memory conversions
 */

#define BYTE_TO_MB(x) ((x) / (1024 * 1024))
#define BYTE_TO_KB(x) (x / 1024)

/*
 * GPU memory specific
 */

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS) - 1)

/*
 * Debug
 */
#ifdef UNICODE
#if defined(DEBUG) || defined(_DEBUG)
#define SCALD_NAME_D3D12_OBJECT(obj, name)           \
    obj->SetName(name);                              \
    OutputDebugStringW(L"::D3D12 Object Created: "); \
    OutputDebugStringW(name);                        \
    OutputDebugStringW(L"\n");
#else
#define SCALD_NAME_D3D12_OBJECT(obj, name)
#endif
#else
// TODO : I still pass the narrow string to SetName which takes the wide one
#if defined(DEBUG) || defined(_DEBUG)
#define SCALD_NAME_D3D12_OBJECT(obj, name)          \
    obj->SetName(name);                             \
    OutputDebugStringA("::D3D12 Object Created: "); \
    OutputDebugStringA(name);                       \
    OutputDebugStringA("\n");
#else
#define SCALD_NAME_D3D12_OBJECT(obj, name)
#endif
#endif

/*
 * Textures
 */

#define TextureMapsMaxCount 128  // 128 textures for every kind of textures

/*
 * Shader resource binding
 */

#define REGISTER_SPACE_0 0
#define REGISTER_SPACE_1 1

#define SHADER_REGISTER(x) (x)