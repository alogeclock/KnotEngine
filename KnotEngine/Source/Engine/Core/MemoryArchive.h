#pragma once

#include "EngineAPI.h"

#include "Core/Archive.h"

#include <span>

// 연속 Byte 배열 끝에 직렬화 데이터를 추가하는 Memory Archive다.
class ENGINE_API FMemoryWriter final : public FArchive
{
public:
	explicit FMemoryWriter(TArray<uint8>& InBytes);

	void Serialize(void* Data, int64 Size) override;
	bool CanSerialize(int64 Size) const override;

private:
	TArray<uint8>& Bytes;
};

// 소유하지 않는 연속 Byte 범위를 안전하게 읽는 Memory Archive다.
class ENGINE_API FMemoryReader final : public FArchive
{
public:
	explicit FMemoryReader(std::span<const uint8> InBytes);

	void Serialize(void* Data, int64 Size) override;
	bool CanSerialize(int64 Size) const override;

private:
	std::span<const uint8> Bytes;
	SIZE_T Offset = 0;
};
