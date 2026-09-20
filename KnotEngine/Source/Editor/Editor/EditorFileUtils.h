#pragma once

#include <filesystem>
#include <optional>

struct FEditorFileDialogOptions
{
	const wchar_t* Filter = L"All Files (*.*)\0*.*\0";
	const wchar_t* Title = L"Select File";
	const wchar_t* DefaultExtension = nullptr;
	const wchar_t* InitialDirectory = nullptr;
	const wchar_t* DefaultFileName = nullptr;
	void* OwnerWindowHandle = nullptr;
	bool bFileMustExist = true;
	bool bPathMustExist = true;
	bool bPromptOverwrite = false;
};

// Editor에서 사용하는 Windows 파일 선택 대화상자를 공통 옵션으로 실행한다.
class FEditorFileUtils final
{
public:
	static std::optional<std::filesystem::path> OpenFileDialog(const FEditorFileDialogOptions& Options);
	static std::optional<std::filesystem::path> SaveFileDialog(const FEditorFileDialogOptions& Options);

private:
	static std::optional<std::filesystem::path> RunFileDialog(const FEditorFileDialogOptions& Options, bool bOpenDialog);
};
