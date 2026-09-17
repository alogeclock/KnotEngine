#pragma once

#include "EngineAPI.h"

#include "Core/Archive.h"
#include "Core/CoreTypes.h"

// .kasset Payload가 표현하는 Runtime Asset의 종류다.
enum class EAssetType : uint32
{
	Unknown,
	StaticMesh,
	Material,
	Texture2D,
};

// 모든 Runtime .kasset 앞에 저장되는 공통 컨테이너 헤더다.
struct ENGINE_API FAssetFileHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'A', 'S', 'T' };
	inline static constexpr uint32 CurrentVersion = 1;

	char Magic[4];
	uint32 ContainerVersion;
	EAssetType AssetType;
	uint32 PayloadVersion;
	uint64 PayloadSize;
};
static_assert(sizeof(FAssetFileHeader) == 24);

inline FArchive& operator<<(FArchive& Ar, FAssetFileHeader& Header)
{
	Ar.Serialize(Header.Magic, sizeof(Header.Magic));
	Ar << Header.ContainerVersion;
	Ar << Header.AssetType;
	Ar << Header.PayloadVersion;
	Ar << Header.PayloadSize;
	return Ar;
}
