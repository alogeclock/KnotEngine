#pragma once

#include "Core/CoreTypes.h"

#include <Windows.h>

class FPaths
{
public:
	static FWString RootDir();
	static FWString ContentDir() { return RootDir() + L"Contents/"; }
	static FWString ShaderDir() { return RootDir() + L"Shaders/"; }
	static FWString SettingDir() { return RootDir() + L"Settings/"; }

	static FWString ImGuiSettingsPath() { return SettingDir() + L"imgui.ini"; }

	static FWString ToWide(const FString& Utf8String);
	static FString ToUtf8(const FWString& WideString);

private:
	static FWString ConvertToWide(const FString& Source, UINT CodePage, DWORD Flags);
};
