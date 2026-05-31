#pragma once

#include <Windows.h>
#include <functional>
#include "Runtime/WindowsWindow.h"
#include "Object/Object.h"

using FOnSizingCallback = std::function<void()>;
using FOnResizingCallback = std::function<void(uint32, uint32)>;

// Windows 애플리케이션과 메시지 루프를 관리하는 객체
// Windows Class 등록, 여러 FWindowsWindow 관리, WndProc에서 HWND → FWindowsWindow 맵핑
class FWindowsApplication
{
public:
    FWindowsApplication() = default;
    ~FWindowsApplication() = default;

	bool Init(HINSTANCE InInstance, int ShowCmd);
    void PumpMessages();
    void Shutdown();

	FWindowsWindow& GetWindow() { return Window; }
    const FWindowsWindow& GetWindow() const { return Window; }

	bool IsExitRequested() const { return bIsExitRequested; }
	bool IsResizing() const { return bIsResizing; }

private:
	HINSTANCE Instance = nullptr;
	FWindowsWindow Window;

	bool bIsExitRequested = false;
    bool bIsResizing = false;

	FOnSizingCallback OnSizingCallback;
	FOnResizingCallback OnResizingCallback;
};
