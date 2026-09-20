#include "Editor/EditorFileUtils.h"

#include <Windows.h>
#include <commdlg.h>
#include <vector>

// Windows 열기 대화상자를 실행하고 선택한 절대 경로를 반환한다.
std::optional<std::filesystem::path> FEditorFileUtils::OpenFileDialog(const FEditorFileDialogOptions& Options)
{
	return RunFileDialog(Options, true);
}

// Windows 저장 대화상자를 실행하고 선택한 절대 경로를 반환한다.
std::optional<std::filesystem::path> FEditorFileUtils::SaveFileDialog(const FEditorFileDialogOptions& Options)
{
	return RunFileDialog(Options, false);
}

// 공통 옵션을 OPENFILENAMEW로 변환해 열기 또는 저장 대화상자를 실행한다.
std::optional<std::filesystem::path> FEditorFileUtils::RunFileDialog(const FEditorFileDialogOptions& Options, bool bOpenDialog)
{
	std::vector<wchar_t> FileBuffer(32768, L'\0');
	if (Options.DefaultFileName)
	{
		wcsncpy_s(FileBuffer.data(), FileBuffer.size(), Options.DefaultFileName, _TRUNCATE);
	}

	OPENFILENAMEW Dialog = {};
	Dialog.lStructSize = sizeof(Dialog);
	Dialog.hwndOwner = static_cast<HWND>(Options.OwnerWindowHandle);
	Dialog.lpstrFilter = Options.Filter;
	Dialog.lpstrTitle = Options.Title;
	Dialog.lpstrFile = FileBuffer.data();
	Dialog.nMaxFile = static_cast<DWORD>(FileBuffer.size());
	Dialog.lpstrInitialDir = Options.InitialDirectory;
	Dialog.lpstrDefExt = Options.DefaultExtension;
	Dialog.nFilterIndex = 1;
	Dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR;
	if (Options.bFileMustExist)
	{
		Dialog.Flags |= OFN_FILEMUSTEXIST;
	}
	if (Options.bPathMustExist)
	{
		Dialog.Flags |= OFN_PATHMUSTEXIST;
	}
	if (Options.bPromptOverwrite)
	{
		Dialog.Flags |= OFN_OVERWRITEPROMPT;
	}

	const BOOL bSelected = bOpenDialog ? GetOpenFileNameW(&Dialog) : GetSaveFileNameW(&Dialog);
	return bSelected ? std::optional<std::filesystem::path>(std::filesystem::path(FileBuffer.data()).lexically_normal()) : std::nullopt;
}
