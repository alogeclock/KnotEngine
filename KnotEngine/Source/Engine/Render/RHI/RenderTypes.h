#pragma once

#include "Core/CoreTypes.h"

struct FBufferHandle
{
	static constexpr uint32 InvalidIndex = static_cast<uint32>(-1);

	uint32 Index = InvalidIndex;
	uint32 Generation = 0;

	bool IsValid() const { return Index != InvalidIndex; }

	void Reset()
	{
		Index = InvalidIndex;
		Generation = 0;
	}

	bool operator==(const FBufferHandle&) const = default;
};

struct FRenderViewport
{
	float TopLeftX = 0.0f;
	float TopLeftY = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	float MinDepth = 0.0f;
	float MaxDepth = 1.0f;
};
