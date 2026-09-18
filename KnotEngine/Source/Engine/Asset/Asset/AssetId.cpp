#include "Asset/Asset/AssetId.h"

#include <Windows.h>
#include <objbase.h>
#include <cstring>
#include <cstdio>

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

FString FAssetId::ToString() const
{
	char Buffer[33] = {};
	std::snprintf(Buffer, sizeof(Buffer), "%016llX%016llX", static_cast<unsigned long long>(High), static_cast<unsigned long long>(Low));
	return Buffer;
}
