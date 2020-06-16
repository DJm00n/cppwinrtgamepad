#include "pch.h"

#include <windows.h>
#include <stdlib.h>
#include <iostream>
#include <thread>
#include <atomic>

extern std::atomic_bool stopGamepadThread;

void GamepadThread();

bool ReconnectIO(bool OpenNewConsole)
{
    bool MadeConsole;

    MadeConsole = false;
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        if (!OpenNewConsole)
            return false;

        MadeConsole = true;
        if (!AllocConsole())
            return false;   // Could throw here
    }

    FILE* fDummy;
    freopen_s(&fDummy, "CONIN$", "r", stdin);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    // C++ streams to console
    std::ios_base::sync_with_stdio();

    _flushall();

    return MadeConsole;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    if (!ReconnectIO(true))
        std::cout << "Started from command prompt\n";

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        switch (msg)
        {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    };
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = "GamingInputExampleClass";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(nullptr, "Window Registration Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        wc.lpszClassName,
        "Windows.Gaming.Input Example",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
        nullptr, nullptr, hInstance, nullptr);

    if (hwnd == nullptr)
    {
        MessageBox(nullptr, "Window Creation Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    std::thread gamepadThread(GamepadThread);

    while (GetMessage(&Msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    stopGamepadThread = true;
    gamepadThread.join();

    std::cout << "Exiting...\n";

    return (int)Msg.wParam;
}