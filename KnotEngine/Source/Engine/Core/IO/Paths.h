#pragma once

class FPaths
{
public:
	static FWString RootDir();
	static FWString ContentDir() { return RootDir() + L"Contents/"; }
	static FWString ShaderDir() { return RootDir() + L"Shaders/"; }	
	static FWString SettingDir() { return RootDir() + L"Settings/"; }

	static FWString ImGuiSettingsFilePath() { return RootDir() + L"Settings/imgui.ini"; }

	static FWString ToWide(const FString& Utf8String);
	static FString ToUtf8(const FWString& WideString);
};
