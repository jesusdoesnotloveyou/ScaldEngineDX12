#include "stdafx.h"
#include "Engine.h"

INT WindowWidth;
INT WindowHeight;

WCHAR WindowTitle[MAX_NAME_STRING];
WCHAR WindowClass[MAX_NAME_STRING];

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    wcscpy_s(WindowTitle, TEXT("Scald Engine"));
    wcscpy_s(WindowClass, TEXT("D3D12SampleClass"));

    WindowWidth = 1280;
    WindowHeight = 720;

    Engine engine(WindowWidth, WindowHeight, WindowTitle, WindowClass);
    return Win32App::Run(&engine, HInstance(), nCmdShow);
}