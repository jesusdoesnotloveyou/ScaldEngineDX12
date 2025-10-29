#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windowsx.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <wrl.h>
#include <shellapi.h>

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <array>
#include <vector>
#include <cassert>

#define MAX_NAME_STRING 256
#define HInstance() GetModuleHandle(NULL)

#ifdef max
	#undef max
#endif

#ifdef min
#undef min
#endif