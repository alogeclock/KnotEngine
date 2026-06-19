#pragma once

#include <Windows.h>
#include <string>

class URenderer;

class FImGuiSystem
{
public:
	void Init(HWND WindowHandle, URenderer& Renderer);
	void BeginFrame();
	void EndFrame();
	void Shutdown();

private:
	std::string ImGuiSettingsPath;
};
