#include "Core/Engine.h"
#include "Core/Win32App.h"

INT WindowWidth;
INT WindowHeight;

WCHAR WindowTitle[MAX_NAME_STRING];
WCHAR WindowClass[MAX_NAME_STRING];

_Use_decl_annotations_ int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    wcscpy_s(WindowTitle, TEXT("Scald Engine"));
    wcscpy_s(WindowClass, TEXT("D3D12SampleClass"));

    WindowWidth = 1280;
    WindowHeight = 720;

    // engine should be separated from WIN32 stuff
    Engine engine(WindowWidth, WindowHeight, WindowTitle, WindowClass);
#ifdef WIN32
    return Win32App::Run(&engine, HInstance(), nCmdShow);
#endif
#ifdef __linux__
    return 0;  // Vulkan or OpenGL
#endif
}

#ifdef CreateWindow
#undef CreateWindow
#endif