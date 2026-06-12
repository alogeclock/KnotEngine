#pragma once

#include "Core/CoreTypes.h"

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
};

struct FVertex
{
	FVector Position;
	FColor Color;
};
