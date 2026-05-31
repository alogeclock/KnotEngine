#pragma once

#include <Windows.h>

// Windows 실제 OS 창(HWND)를 감싸는 엔진 래퍼 객체, 창 크기/상태/타이틀/핸들 관리
class FWindowsWindow
{
public:
	FWindowsWindow() = default;
	~FWindowsWindow() = default;

	void Init(HWND InHWindow);
    HWND GetHwnd() const { return HWindow; }

	float GetWidth() const { return Width; }
    float GetHeight() const { return Height; }

	void OnResized(uint32 InWidth, uint32 InHeight);
	POINT ToClientPoint(POINT ScreenPoint) const;

private:
	HWND HWindow = nullptr;
	float Width = 0.0f;
	float Height = 0.0f;
};
