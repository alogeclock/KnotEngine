#include "WindowsApplication.h"
#include "resource.h"

#include "Core/Assert.h"

#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

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

void FWindowsApplication::Startup(HINSTANCE InInstance, int ShowCmd)
{
	checkf(InInstance, "FWindowsApplication::Startup에 전달된 HINSTANCE가 null이다.");
	Instance = InInstance;

	WCHAR ClassName[] = L"KnotWindowClass";
	WCHAR Title[] = L"KnotEngine";

	WNDCLASSEXW WindowClass = {};
	WindowClass.cbSize = sizeof(WNDCLASSEXW);
	WindowClass.lpfnWndProc = WndProc;
	WindowClass.hInstance = Instance;
	WindowClass.hIcon = static_cast<HICON>(LoadImageW(Instance, MAKEINTRESOURCEW(IDI_KNOTENGINE), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR | LR_SHARED));
	WindowClass.hIconSm = static_cast<HICON>(LoadImageW(Instance, MAKEINTRESOURCEW(IDI_KNOTENGINE), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED));
	WindowClass.lpszClassName = ClassName;

	panicf(RegisterClassExW(&WindowClass), "RegisterClassExW 실패. GetLastError()={}", GetLastError());

	HWND WindowHandle = CreateWindowExW(0, ClassName, Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, Instance, nullptr);
	panicf(WindowHandle, "CreateWindowExW 실패. GetLastError()={}", GetLastError());

	Window.Startup(WindowHandle);
	ShowWindow(WindowHandle, ShowCmd);
	UpdateWindow(WindowHandle);
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
