#include "WindowsWindow.h"

#include "Core/Assert.h"

void FWindowsWindow::Startup(HWND InHWindow)
{
	checkf(InHWindow, "FWindowsWindow::Startup에 전달된 HWND가 null이다.");
	HWindow = InHWindow;

	RECT Rect = {};
	panicf(GetClientRect(HWindow, &Rect), "GetClientRect 실패. GetLastError()={}", GetLastError());
	Width = static_cast<float>(Rect.right - Rect.left);
	Height = static_cast<float>(Rect.bottom - Rect.top);
}

void FWindowsWindow::OnResized(uint32 InWidth, uint32 InHeight)
{
	Width = static_cast<float>(InWidth);
	Height = static_cast<float>(InHeight);
}

POINT FWindowsWindow::ToClientPoint(POINT ScreenPoint) const
{
	check(HWindow);
	verify(ScreenToClient(HWindow, &ScreenPoint));
	return ScreenPoint;
}
