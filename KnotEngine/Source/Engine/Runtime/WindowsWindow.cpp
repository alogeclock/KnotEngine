#include "WindowsWindow.h"

void FWindowsWindow::Init(HWND InHWindow)
{
	HWindow = InHWindow;

	RECT Rect;
	GetClientRect(HWindow, &Rect);
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
	ScreenToClient(HWindow, &ScreenPoint);
	return ScreenPoint;
}