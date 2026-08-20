#include "WindowsApplication.h"
#include "resource.h"

#include "Core/Assert.h"

#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

LRESULT CALLBACK FWindowsApplication::WindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	FWindowsApplication* Application = reinterpret_cast<FWindowsApplication*>(GetWindowLongPtrW(WindowHandle, GWLP_USERDATA));
	if (Message == WM_NCCREATE)
	{
		const CREATESTRUCTW* CreateInfo = reinterpret_cast<const CREATESTRUCTW*>(LParam);
		Application = static_cast<FWindowsApplication*>(CreateInfo->lpCreateParams);
		SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Application));
	}

	return Application ? Application->ProcessMessage(WindowHandle, Message, WParam, LParam) : DefWindowProcW(WindowHandle, Message, WParam, LParam);
}

// OS 입력은 ImGui 소비 여부와 무관하게 먼저 수집하고, 창 수명과 크기 메시지는 애플리케이션에서 처리한다.
LRESULT FWindowsApplication::ProcessMessage(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
	WindowsInput.ProcessMessage(WindowHandle, Message, WParam, LParam);
	if (ImGui_ImplWin32_WndProcHandler(WindowHandle, Message, WParam, LParam))
	{
		return true;
	}

	switch (Message)
	{
	case WM_ENTERSIZEMOVE:
		bIsResizing = true;
		break;
	case WM_EXITSIZEMOVE:
		bIsResizing = false;
		if (OnResizingCallback && Window.GetHwnd())
		{
			OnResizingCallback(static_cast<uint32>(Window.GetWidth()), static_cast<uint32>(Window.GetHeight()));
		}
		break;
	case WM_SIZING:
		if (OnSizingCallback)
		{
			OnSizingCallback();
		}
		break;
	case WM_SIZE:
		if (Window.GetHwnd() == WindowHandle)
		{
			const uint32 Width = static_cast<uint32>(LOWORD(LParam));
			const uint32 Height = static_cast<uint32>(HIWORD(LParam));
			Window.OnResized(Width, Height);
			if (!bIsResizing && OnResizingCallback)
			{
				OnResizingCallback(Width, Height);
			}
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_UNICHAR:
		if (WParam == UNICODE_NOCHAR)
		{
			return true;
		}
		break;
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_XBUTTONDBLCLK:
		return true;
	case WM_NCDESTROY:
		SetWindowLongPtrW(WindowHandle, GWLP_USERDATA, 0);
		break;
	default:
		break;
	}
	return DefWindowProcW(WindowHandle, Message, WParam, LParam);
}

void FWindowsApplication::Startup(HINSTANCE InInstance, int ShowCmd)
{
	checkf(InInstance, "FWindowsApplication::Startup에 전달된 HINSTANCE가 null이다.");
	Instance = InInstance;

	static constexpr WCHAR ClassName[] = L"KnotWindowClass";
	static constexpr WCHAR Title[] = L"KnotEngine";

	WNDCLASSEXW WindowClass = {};
	WindowClass.cbSize = sizeof(WNDCLASSEXW);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.hInstance = Instance;
	WindowClass.hIcon = static_cast<HICON>(LoadImageW(Instance, MAKEINTRESOURCEW(IDI_KNOTENGINE), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR | LR_SHARED));
	WindowClass.hIconSm = static_cast<HICON>(LoadImageW(Instance, MAKEINTRESOURCEW(IDI_KNOTENGINE), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR | LR_SHARED));
	WindowClass.lpszClassName = ClassName;

	panicf(RegisterClassExW(&WindowClass), "RegisterClassExW 실패. GetLastError()={}", GetLastError());

	HWND WindowHandle = CreateWindowExW(0, ClassName, Title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, Instance, this);
	panicf(WindowHandle, "CreateWindowExW 실패. GetLastError()={}", GetLastError());

	Window.Startup(WindowHandle);
	WindowsInput.Startup(WindowHandle);
	ShowWindow(WindowHandle, ShowCmd);
	UpdateWindow(WindowHandle);
}

void FWindowsApplication::PumpMessages()
{
	MSG Message = {};
	while (PeekMessageW(&Message, nullptr, 0, 0, PM_REMOVE))
	{
		if (Message.message == WM_QUIT)
		{
			bIsExitRequested = true;
			break;
		}

		TranslateMessage(&Message);
		DispatchMessageW(&Message);
	}
	InputSnapshot = WindowsInput.TakeSnapshot();
}

void FWindowsApplication::Shutdown()
{
	WindowsInput.Shutdown();
	if (Window.GetHwnd() && IsWindow(Window.GetHwnd()))
	{
		DestroyWindow(Window.GetHwnd());
	}
}
