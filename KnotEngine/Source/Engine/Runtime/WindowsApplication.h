#pragma once

#include "Core/CoreTypes.h"
#include "Input/InputSnapshot.h"
#include "Runtime/WindowsInput.h"
#include "Runtime/WindowsWindow.h"

#include <Windows.h>
#include <functional>

using FOnSizingCallback = std::function<void()>;
using FOnResizingCallback = std::function<void(uint32, uint32)>;

// Windows 애플리케이션과 메시지 루프를 관리하는 객체
// Windows Class 등록, 여러 FWindowsWindow 관리, WndProc에서 HWND → FWindowsWindow 맵핑
class FWindowsApplication
{
public:
	FWindowsApplication() = default;
	~FWindowsApplication() = default;

	void Startup(HINSTANCE InInstance, int ShowCmd);
	void PumpMessages();
	void Shutdown();

	FWindowsWindow& GetWindow() { return Window; }
	const FWindowsWindow& GetWindow() const { return Window; }
	const FInputSnapshot& GetInputSnapshot() const { return InputSnapshot; }

	void RequestExit() { bIsExitRequested = true; }
	bool IsExitRequested() const { return bIsExitRequested; }
	bool IsResizing() const { return bIsResizing; }

private:
	static LRESULT CALLBACK WindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);
	LRESULT ProcessMessage(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam);

	HINSTANCE Instance = nullptr;
	FWindowsWindow Window;
	FWindowsInput WindowsInput;
	FInputSnapshot InputSnapshot;

	bool bIsExitRequested = false;
	bool bIsResizing = false;

	FOnSizingCallback OnSizingCallback;
	FOnResizingCallback OnResizingCallback;
};
