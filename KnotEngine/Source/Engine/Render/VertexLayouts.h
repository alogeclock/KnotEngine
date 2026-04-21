#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"

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
