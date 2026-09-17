#include "Core/MemoryArchive.h"

#include <cstring>

FMemoryWriter::FMemoryWriter(TArray<uint8>& InBytes)
	: Bytes(InBytes)
{
	bIsSaving = true;
	bIsPersistent = true;
}

void FMemoryWriter::Serialize(void* Data, int64 Size)
{
	if (HasError() || !CanSerialize(Size) || (Size > 0 && !Data))
	{
		SetError();
		return;
	}
	if (Size == 0)
	{
		return;
	}

	const SIZE_T Offset = Bytes.size();
	Bytes.resize(Offset + static_cast<SIZE_T>(Size));
	std::memcpy(Bytes.data() + Offset, Data, static_cast<SIZE_T>(Size));
}

bool FMemoryWriter::CanSerialize(int64 Size) const
{
	if (Size < 0)
	{
		return false;
	}

	const SIZE_T WriteSize = static_cast<SIZE_T>(Size);
	return WriteSize <= Bytes.max_size() - Bytes.size();
}

FMemoryReader::FMemoryReader(std::span<const uint8> InBytes)
	: Bytes(InBytes)
{
	bIsLoading = true;
	bIsPersistent = true;
}

void FMemoryReader::Serialize(void* Data, int64 Size)
{
	if (HasError() || !CanSerialize(Size) || (Size > 0 && !Data))
	{
		SetError();
		return;
	}
	if (Size == 0)
	{
		return;
	}

	std::memcpy(Data, Bytes.data() + Offset, static_cast<SIZE_T>(Size));
	Offset += static_cast<SIZE_T>(Size);
}

bool FMemoryReader::CanSerialize(int64 Size) const
{
	return Size >= 0 && static_cast<uint64>(Size) <= Bytes.size() - Offset;
}
