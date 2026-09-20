#include "Editor/EditorFileUtils.h"

#include "Core/Log.h"

#include <Windows.h>
#include <shobjidl_core.h>
#include <vector>
#include <wrl/client.h>

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

// 공통 옵션으로 Windows Shell 열기 또는 저장 대화상자를 실행한다.
std::optional<std::filesystem::path> FEditorFileUtils::RunFileDialog(const FEditorFileDialogOptions& Options, bool bOpenDialog)
{
	const HRESULT InitializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	const bool bShouldUninitialize = SUCCEEDED(InitializeResult);
	if (FAILED(InitializeResult) && InitializeResult != RPC_E_CHANGED_MODE)
	{
		KE_LOG(LogEditorFileUtils, Error, "파일 대화상자 COM 초기화에 실패했다. HRESULT={:#x}", static_cast<uint32>(InitializeResult));
		return std::nullopt;
	}

	Microsoft::WRL::ComPtr<IFileDialog> Dialog;
	const CLSID& DialogClass = bOpenDialog ? CLSID_FileOpenDialog : CLSID_FileSaveDialog;
	HRESULT Result = CoCreateInstance(DialogClass, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Dialog.GetAddressOf()));
	if (FAILED(Result))
	{
		KE_LOG(LogEditorFileUtils, Error, "파일 대화상자 생성에 실패했다. HRESULT={:#x}", static_cast<uint32>(Result));
		if (bShouldUninitialize)
		{
			CoUninitialize();
		}
		return std::nullopt;
	}

	FILEOPENDIALOGOPTIONS DialogFlags = FOS_FORCEFILESYSTEM;
	if (Options.bFileMustExist)
	{
		DialogFlags |= FOS_FILEMUSTEXIST;
	}
	if (Options.bPathMustExist)
	{
		DialogFlags |= FOS_PATHMUSTEXIST;
	}
	if (Options.bPromptOverwrite)
	{
		DialogFlags |= FOS_OVERWRITEPROMPT;
	}
	Dialog->SetOptions(DialogFlags);
	Dialog->SetTitle(Options.Title);
	Dialog->SetDefaultExtension(Options.DefaultExtension);
	if (Options.DefaultFileName)
	{
		Dialog->SetFileName(Options.DefaultFileName);
	}

	std::vector<COMDLG_FILTERSPEC> FileTypes;
	for (const wchar_t* Name = Options.Filter; Name && *Name;)
	{
		const wchar_t* Pattern = Name + wcslen(Name) + 1;
		if (!*Pattern)
		{
			break;
		}
		FileTypes.push_back({ Name, Pattern });
		Name = Pattern + wcslen(Pattern) + 1;
	}
	if (!FileTypes.empty())
	{
		Dialog->SetFileTypes(static_cast<UINT>(FileTypes.size()), FileTypes.data());
	}

	if (Options.InitialDirectory)
	{
		Microsoft::WRL::ComPtr<IShellItem> InitialFolder;
		Result = SHCreateItemFromParsingName(Options.InitialDirectory, nullptr, IID_PPV_ARGS(InitialFolder.GetAddressOf()));
		if (SUCCEEDED(Result))
		{
			Dialog->SetFolder(InitialFolder.Get());
		}
	}

	Result = Dialog->Show(static_cast<HWND>(Options.OwnerWindowHandle));
	if (Result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
	{
		Dialog.Reset();
		if (bShouldUninitialize)
		{
			CoUninitialize();
		}
		return std::nullopt;
	}
	if (FAILED(Result))
	{
		KE_LOG(LogEditorFileUtils, Error, "파일 대화상자 표시 중 오류가 발생했다. HRESULT={:#x}", static_cast<uint32>(Result));
		Dialog.Reset();
		if (bShouldUninitialize)
		{
			CoUninitialize();
		}
		return std::nullopt;
	}

	Microsoft::WRL::ComPtr<IShellItem> SelectedItem;
	PWSTR SelectedPath = nullptr;
	Result = Dialog->GetResult(SelectedItem.GetAddressOf());
	if (SUCCEEDED(Result))
	{
		Result = SelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &SelectedPath);
	}
	std::optional<std::filesystem::path> FilePath;
	if (SUCCEEDED(Result) && SelectedPath)
	{
		FilePath = std::filesystem::path(SelectedPath).lexically_normal();
	}
	CoTaskMemFree(SelectedPath);
	SelectedItem.Reset();
	Dialog.Reset();
	if (bShouldUninitialize)
	{
		CoUninitialize();
	}
	return FilePath;
}
