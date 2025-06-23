#include "stdafx.h"
#include "Engine.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Engine engine(1280, 720, L"Scald Engine Window");
    return Win32App::Run(&engine, hInstance, nCmdShow);
}