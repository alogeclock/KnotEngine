#include "Asset/Asset/AssetId.h"

#include <Windows.h>
#include <objbase.h>
#include <cstring>
#include <cstdio>
#include <charconv>
#include <system_error>

// Windows가 생성한 UUID를 경로와 무관한 Asset ID로 변환한다.
FAssetId FAssetId::New()
{
	GUID Guid = {};
	panicf(SUCCEEDED(CoCreateGuid(&Guid)), "Asset ID 생성에 실패했다.");

	FAssetId Result;
	static_assert(sizeof(Result) == sizeof(Guid));
	std::memcpy(&Result, &Guid, sizeof(Result));
	panic(Result.IsValid());
	return Result;
}

// 32자리 16진수 문자열을 영속 Asset ID로 복원한다.
bool FAssetId::TryParse(std::string_view Text, FAssetId& OutAssetId)
{
	if (Text.size() != 32)
	{
		return false;
	}

	FAssetId Parsed;
	const char* Middle = Text.data() + 16;
	const auto HighResult = std::from_chars(Text.data(), Middle, Parsed.High, 16);
	const auto LowResult = std::from_chars(Middle, Text.data() + Text.size(), Parsed.Low, 16);
	if (HighResult.ec != std::errc() || HighResult.ptr != Middle || LowResult.ec != std::errc() || LowResult.ptr != Text.data() + Text.size())
	{
		return false;
	}
	OutAssetId = Parsed;
	return true;
}

FString FAssetId::ToString() const
{
	char Buffer[33] = {};
	std::snprintf(Buffer, sizeof(Buffer), "%016llX%016llX", static_cast<unsigned long long>(High), static_cast<unsigned long long>(Low));
	return Buffer;
}
