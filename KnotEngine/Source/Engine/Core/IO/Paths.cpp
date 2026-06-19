#include "Paths.h"

namespace
{
	FWString ConvertToWide(const FString& Source, UINT CodePage, DWORD Flags)
	{
		if (Source.empty())
		{
			return {};
		}

		const int32 Size = MultiByteToWideChar(CodePage, Flags, Source.c_str(), -1, nullptr, 0);
		if (Size <= 0)
		{
			return {};
		}

		FWString Result(static_cast<size_t>(Size - 1), L'\0');
		MultiByteToWideChar(CodePage, Flags, Source.c_str(), -1, Result.data(), Size);
		return Result;
	}
}

// 배포 환경과 개발 환경을 구분하여 루트 디렉토리를 반환합니다.
FWString FPaths::RootDir()
{
	static FWString Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();

		if (std::filesystem::exists(ExeDir / L"Shaders"))
		{
			Cached = ExeDir.generic_wstring() + L"/";
		}
		else
		{
			bool bFound = false;
			std::filesystem::path SearchDir = ExeDir;

			while (SearchDir.has_parent_path())
			{
				SearchDir = SearchDir.parent_path();

				if (std::filesystem::exists(SearchDir / L"Shaders"))
				{
					Cached = SearchDir.generic_wstring() + L"/";
					bFound = true;
					break;
				}

				if (SearchDir == SearchDir.root_path())
				{
					break;
				}
			}

			if (!bFound)
			{
				Cached = std::filesystem::current_path().generic_wstring() + L"/";
			}
		}
	}
	return Cached;
}

// UTF-8 문자열을 Wide 문자열로 변환합니다. 먼저 CP_UTF8로 시도하고, 실패하면 CP_ACP로 시도.
FWString FPaths::ToWide(const FString& Utf8String)
{
	if (Utf8String.empty()) return {};

	FWString Result = ConvertToWide(Utf8String, CP_UTF8, MB_ERR_INVALID_CHARS);
	if (!Result.empty()) return Result;

	return ConvertToWide(Utf8String, CP_ACP, 0);
}

// Wide 문자열을 UTF-8 문자열로 변환, 엔진 전역에서 사용되는 경로 유틸리티.
FString FPaths::ToUtf8(const FWString& WideString)
{
	if (WideString.empty()) return {};

	const int32 Size = WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (Size <= 0) return {};

	FString Result(static_cast<size_t>(Size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, Result.data(), Size, nullptr, nullptr);
	return Result;
}
