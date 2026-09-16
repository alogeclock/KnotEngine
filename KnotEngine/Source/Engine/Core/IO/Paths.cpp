#include "Paths.h"

#include "Core/Assert.h"

#include <algorithm>
#include <filesystem>

// 지정된 코드 페이지를 사용해 문자열을 Wide 문자열로 변환합니다.
FWString FPaths::ConvertToWide(const FString& Source, UINT CodePage, DWORD Flags)
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

// 배포 환경과 개발 환경을 구분하여 루트 디렉토리를 반환한다.
FWString FPaths::RootDir()
{
	static FWString Cached;
	if (Cached.empty())
	{
		WCHAR Buffer[MAX_PATH];
		GetModuleFileNameW(nullptr, Buffer, MAX_PATH);
		std::filesystem::path ExeDir = std::filesystem::path(Buffer).parent_path();

#if defined(KNOT_BUILD_SHIPPING)
		Cached = ExeDir.generic_wstring() + L"/";
#else
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
#endif
	}
	return Cached;
}

// Development, Debug 빌드 구성일 경우 루트의 Saved 디렉토리에 저장하고,
// Shipping 빌드 구성일 경우 실행 파일 위치의 Saved 디렉토리에 저장한다.
FWString FPaths::SavedDir()
{
	static FWString Cached;
	if (!Cached.empty())
	{
		return Cached;
	}

#if defined(KNOT_BUILD_DEBUG) || defined(KNOT_BUILD_DEVELOPMENT)
	std::filesystem::path SearchDir = std::filesystem::path(RootDir());
	while (true)
	{
		const bool bIsProjectRoot =
			std::filesystem::exists(SearchDir / L"Source") &&
			std::filesystem::exists(SearchDir / L"Build" / L"CMake" / L"CMakeLists.txt");
		if (bIsProjectRoot)
		{
			Cached = (SearchDir / L"Saved").generic_wstring() + L"/";
			return Cached;
		}

		const std::filesystem::path ParentDir = SearchDir.parent_path();
		if (ParentDir.empty() || ParentDir == SearchDir)
		{
			break;
		}
		SearchDir = ParentDir;
	}
#endif

	Cached = RootDir() + L"Saved/";
	return Cached;
}

// 논리 경로의 구분자와 끝 구분자를 엔진 경로 형식으로 정규화한다.
FString FPaths::Normalize(const FString& Path)
{
	FString Result = Path;
	std::replace(Result.begin(), Result.end(), '\\', '/');
	while (Result.size() > 1 && Result.ends_with('/'))
	{
		Result.pop_back();
	}
	return Result;
}

// 논리 경로에서 마지막 이름을 제외한 부모 경로를 반환한다.
FString FPaths::GetPath(const FString& Path)
{
	const FString NormalizedPath = Normalize(Path);
	const SIZE_T Separator = NormalizedPath.find_last_of('/');
	if (Separator == FString::npos)
	{
		return {};
	}
	return Separator == 0 ? "/" : NormalizedPath.substr(0, Separator);
}

// 두 논리 경로를 중복 구분자 없이 결합한다.
FString FPaths::Combine(const FString& Left, const FString& Right)
{
	const FString NormalizedLeft = Normalize(Left);
	FString NormalizedRight = Normalize(Right);
	while (NormalizedRight.starts_with('/'))
	{
		NormalizedRight.erase(NormalizedRight.begin());
	}
	if (NormalizedLeft.empty())
	{
		return NormalizedRight;
	}
	if (NormalizedRight.empty())
	{
		return NormalizedLeft;
	}
	return NormalizedLeft == "/" ? "/" + NormalizedRight : NormalizedLeft + "/" + NormalizedRight;
}

// 경로가 부모 경로와 같거나 부모 아래에 위치하는지 경로 구분자 단위로 확인한다.
bool FPaths::IsInside(const FString& Path, const FString& ParentPath)
{
	const FString NormalizedPath = Normalize(Path);
	const FString NormalizedParent = Normalize(ParentPath);
	if (NormalizedPath == NormalizedParent)
	{
		return true;
	}
	if (NormalizedParent == "/")
	{
		return NormalizedPath.starts_with('/');
	}
	return NormalizedPath.starts_with(NormalizedParent + "/");
}

// UTF-8 문자열을 Wide 문자열로 변환한다. 먼저 CP_UTF8로 시도하고, 실패하면 CP_ACP로 시도.
FWString FPaths::ToWide(const FString& Utf8String)
{
	if (Utf8String.empty())
	{
		return {};
	}

	FWString Result = ConvertToWide(Utf8String, CP_UTF8, MB_ERR_INVALID_CHARS);
	if (!Result.empty())
	{
		return Result;
	}

	return ConvertToWide(Utf8String, CP_ACP, 0);
}

// Wide 문자열을 UTF-8 문자열로 변환, 엔진 전역에서 사용되는 경로 유틸리티.
FString FPaths::ToUtf8(const FWString& WideString)
{
	if (WideString.empty())
	{
		return {};
	}

	const int32 Size = WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (Size <= 0)
	{
		return {};
	}

	FString Result(static_cast<size_t>(Size - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, WideString.c_str(), -1, Result.data(), Size, nullptr, nullptr);
	return Result;
}

// Contents 기준 논리 경로를 정규화하고 Contents 외부로 벗어나지 않는 실제 경로로 변환한다.
std::filesystem::path FPaths::ResolveContentPath(const FString& LogicalPath)
{
	const FString NormalizedPath = Normalize(LogicalPath);
	const FString RelativePath = NormalizedPath.starts_with('/') ? NormalizedPath.substr(1) : NormalizedPath;
	const std::filesystem::path ContentRoot = std::filesystem::path(ContentDir()).lexically_normal();
	const std::filesystem::path ResolvedPath = (ContentRoot / ToWide(RelativePath)).lexically_normal();
	const std::filesystem::path RelativeToRoot = ResolvedPath.lexically_relative(ContentRoot);
	const bool bOutsideContent = RelativeToRoot.empty() || RelativeToRoot.is_absolute() ||
	                             (RelativeToRoot.begin() != RelativeToRoot.end() && *RelativeToRoot.begin() == L"..");
	panicf(!bOutsideContent, "Content 외부 경로를 사용할 수 없다. Path={}", LogicalPath);
	return ResolvedPath;
}
