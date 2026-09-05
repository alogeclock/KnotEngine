#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

#include <Windows.h>

class ENGINE_API FPaths
{
public:
	static FWString RootDir();
	static FWString ContentDir() { return RootDir() + L"Contents/"; }
	static FWString ShaderDir() { return RootDir() + L"Shaders/"; }
	static FWString SettingDir() { return RootDir() + L"Settings/"; }
	static FWString SavedDir();
	static FWString LogDir() { return SavedDir() + L"Logs/"; }
	static FWString ImGuiSettingsPath() { return SettingDir() + L"imgui.ini"; }

	static FWString ToWide(const FString& Utf8String);
	static FString ToUtf8(const FWString& WideString);

private:
	static FWString ConvertToWide(const FString& Source, UINT CodePage, DWORD Flags);
};
