#pragma once

#include "EngineAPI.h"

#include "Core/Archive.h"
#include "Core/CoreTypes.h"

// 경로와 독립적으로 Runtime Asset을 영속 식별하는 128-bit ID다.
struct ENGINE_API FAssetId
{
	uint64 High = 0;
	uint64 Low = 0;

	static FAssetId New();

	bool IsValid() const { return High != 0 || Low != 0; }
	FString ToString() const;

	friend bool operator==(const FAssetId&, const FAssetId&) = default;
};
static_assert(sizeof(FAssetId) == 16);

inline FArchive& operator<<(FArchive& Ar, FAssetId& AssetId)
{
	Ar << AssetId.High;
	Ar << AssetId.Low;
	return Ar;
}

struct FAssetIdHash
{
	SIZE_T operator()(const FAssetId& AssetId) const
	{
		return std::hash<uint64>{}(AssetId.High) ^ (std::hash<uint64>{}(AssetId.Low) << 1);
	}
};
