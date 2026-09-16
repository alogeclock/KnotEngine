#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

#include <Windows.h>
#include <filesystem>

class ENGINE_API FPaths
{
public:
	static FWString RootDir();
	static FWString ContentDir() { return RootDir() + L"Contents/"; }
	static FWString ShaderDir() { return ContentDir() + L"Engine/Shaders/"; }
	static FWString SavedDir();
	static FWString ConfigDir() { return SavedDir() + L"Config/"; }
	static FWString LogDir() { return SavedDir() + L"Logs/"; }

	static FWString ImGuiSettingsPath() { return ConfigDir() + L"imgui.ini"; }
	static FString GetPath(const FString& Path);
	static FString Combine(const FString& Left, const FString& Right);
	static bool IsInside(const FString& Path, const FString& ParentPath);

	static FWString ToWide(const FString& Utf8String);
	static FString ToUtf8(const FWString& WideString);

	static std::filesystem::path ResolveContentPath(const FString& LogicalPath);

private:
	static FString Normalize(const FString& Path);
	static FWString ConvertToWide(const FString& Source, UINT CodePage, DWORD Flags);
};
