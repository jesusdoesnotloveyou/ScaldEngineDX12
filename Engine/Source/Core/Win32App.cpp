#include "Win32App.h"
#include "D3D12Sample.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

HWND Win32App::m_hwnd = nullptr;

extern "C++" IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int Win32App::Run(D3D12Sample* pSample, HINSTANCE hInstance, int nCmdShow)
{
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    // Parse the command line parameters
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    pSample->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class.
    WNDCLASSEX windowClass = {0};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = pSample->GetWindowClass();
    RegisterClassEx(&windowClass);

    RECT windowRect = {0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight())};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(windowClass.lpszClassName, pSample->GetTitle(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,  // We have no parent window.
        nullptr,  // We aren't using menus.
        hInstance, pSample);

    // Initialize the sample. OnInit is defined in each child-implementation of D3D12Sample.
    pSample->OnInit();

    ShowWindow(m_hwnd, nCmdShow /* The same as SW_SHOW */);

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;  // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);

    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0u;
    rid.hwndTarget = NULL;

    if (RegisterRawInputDevices(&rid, 1u, sizeof(rid)) == FALSE)
    {
#if defined(DEBUG) || defined(_DEBUG)
        OutputDebugString(L"[ERROR] Failed to register Raw Input Device");
#endif
    }

    return pSample->Run();
}

// Main message handler for the sample.
LRESULT CALLBACK Win32App::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam)) return true;

    D3D12Sample* pSample = reinterpret_cast<D3D12Sample*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
        case WM_ACTIVATE:
        {
            if (LOWORD(wParam) == WA_INACTIVE)
            {
                pSample->Pause();
            }
            else
            {
                pSample->UnPause();
            }
            return 0;
        }

        case WM_SIZE:
        {
            pSample->SetWidth(LOWORD(lParam));
            pSample->SetHeight(HIWORD(lParam));

            if (pSample->IsDeviceValid())
            {
                if (wParam == SIZE_MINIMIZED)
                {
                    pSample->Minimize();
                }
                else if (wParam == SIZE_MAXIMIZED)
                {
                    pSample->Maximize();
                    pSample->OnResize();
                }
                else if (wParam == SIZE_RESTORED)
                {
                    // Restoring from minimized state?
                    if (IsMinimized(hWnd))
                    {
                        pSample->RestoreSize(true);
                        pSample->OnResize();
                    }

                    // Restoring from maximized state?
                    else if (IsMaximized(hWnd))
                    {
                        pSample->RestoreSize(false);
                        pSample->OnResize();
                    }
                    else if (pSample->IsResizing())
                    {
                        // If user is dragging the resize bars, we do not resize
                        // the buffers here because as the user continuously
                        // drags the resize bars, a stream of WM_SIZE messages are
                        // sent to the window, and it would be pointless (and slow)
                        // to resize for each WM_SIZE message received from dragging
                        // the resize bars.  So instead, we reset after the user is
                        // done resizing the window and releases the resize bars, which
                        // sends a WM_EXITSIZEMOVE message.
                    }
                    else  // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                    {
                        pSample->OnResize();
                    }
                }
            }
            return 0;
        }

        // WM_ENTERSIZEMOVE is sent when the user grabs the resize bars.
        case WM_ENTERSIZEMOVE:
        {
            pSample->Pause();
            pSample->SetResizing(true);
            return 0;
        }

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
        case WM_EXITSIZEMOVE:
        {
            pSample->UnPause();
            pSample->SetResizing(false); 
            pSample->OnResize();
            return 0;
        }

        case WM_CREATE:
        {
            // Save the D3D12Sample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
            return 0;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        // Catch this message so to prevent the window from becoming too small.
        case WM_GETMINMAXINFO:
            ((MINMAXINFO*)lParam)->ptMinTrackSize.x = LONG(200);
            ((MINMAXINFO*)lParam)->ptMinTrackSize.y = LONG(200);
            return 0;

        /******************************** INPUT ********************************/
        /* Keyboard */
        case WM_KEYDOWN:
        {
            if (pSample)
            {
                if (wParam == VK_F1)
                {
                    pSample->Set4xMsaaState(!pSample->Get4xMsaaState());
                }

                pSample->OnKeyDown(static_cast<UINT8>(wParam));
            }
            break;
        }

        case WM_KEYUP:
        {
            if (wParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
                return 0;
            }

            if (pSample)
            {
                pSample->OnKeyUp(static_cast<UINT8>(wParam));
            }
            break;
        }

        /* Mouse */
        case WM_LBUTTONDOWN:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnLeftPressed(pt.x, pt.y);
            break;
        }

        case WM_MBUTTONDOWN:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnMiddlePressed(pt.x, pt.y);
            break;
        }
        case WM_RBUTTONDOWN:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnRightPressed(pt.x, pt.y);
            break;
        }

        case WM_LBUTTONUP:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnLeftReleased(pt.x, pt.y);
            break;
        }
        case WM_MBUTTONUP:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnMiddleReleased(pt.x, pt.y);
            break;
        }
        case WM_RBUTTONUP:
        {
            const POINTS pt = MAKEPOINTS(lParam);
            pSample->GetMouse()->OnRightReleased(pt.x, pt.y);
            break;
        }

        case WM_MOUSEMOVE:
        {
            const int x = LOWORD(lParam);
            const int y = HIWORD(lParam);
            pSample->GetMouse()->OnMouseMove(x, y);
#if defined(DEBUG) || defined(_DEBUG)
           std::wstring rawInputInfoDefug = L"{X,Y}: " + std::to_wstring(x) + L" " + std::to_wstring(y) + L"\n";
           OutputDebugString(rawInputInfoDefug.c_str());
#endif
            break;
        }

        case WM_MOUSEWHEEL:
        {
            const int x = LOWORD(lParam);
            const int y = HIWORD(lParam);

            pSample->GetMouse()->OnWheelDelta(x, y, GET_WHEEL_DELTA_WPARAM(wParam));
            break;
        }

        /******************************** RAW INPUT ********************************/
        case WM_INPUT:
        {
            UINT dataSize = {};
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));

            if (dataSize > 0)
            {
                // TO DO: Bad, heap alloc.
                const auto m_rawBuffer = std::make_unique<BYTE[]>(dataSize);
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, m_rawBuffer.get(), &dataSize, sizeof(RAWINPUTHEADER)) == dataSize)
                {
                    auto ri = reinterpret_cast<RAWINPUT*>(m_rawBuffer.get());
                    if (ri->header.dwType == RIM_TYPEMOUSE)
                    {
                        pSample->GetMouse()->OnMouseMoveRaw(ri->data.mouse.lLastX, ri->data.mouse.lLastY);
#if defined(DEBUG) || defined(_DEBUG)
                        std::wstring rawInputInfoDefug = L"Delta {X,Y}: " + std::to_wstring(ri->data.mouse.lLastX) + L" " + std::to_wstring(ri->data.mouse.lLastY) + L"\n";
                        OutputDebugString(rawInputInfoDefug.c_str());
#endif
                    }
                }
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}