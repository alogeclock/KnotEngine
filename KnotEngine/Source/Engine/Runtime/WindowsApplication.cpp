#include "WindowsApplication.h"

#include <cassert>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

bool FWindowsApplication::Init(HINSTANCE InInstance, int ShowCmd)
{
	Instance = InInstance;

	WCHAR ClassName[] = L"KnotWindowClass";
	WCHAR Title[] = L"KnotEngine";

	WNDCLASSW WindowClass = {};
	WindowClass.lpfnWndProc = WndProc;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = ClassName;

	RegisterClassW(&WindowClass);

	HWND WindowHandle = CreateWindowExW(0, ClassName, Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, Instance, nullptr);

	Window.Init(WindowHandle);
	ShowWindow(WindowHandle, ShowCmd);
	UpdateWindow(WindowHandle);

	return true;
}

void FWindowsApplication::PumpMessages()
{
	MSG Message = {};
	while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
	{
		if (Message.message == WM_QUIT)
		{
			bIsExitRequested = true;
			break;
		}

		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
}

void FWindowsApplication::Shutdown()
{
	if (Window.GetHwnd() && IsWindow(Window.GetHwnd()))
	{
		DestroyWindow(Window.GetHwnd());
	}
}
