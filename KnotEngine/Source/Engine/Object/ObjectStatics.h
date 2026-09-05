#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

class ENGINE_API ObjectStatics
{
public:
	static uint32 GenerateUUID() { return NextUUID++; }

private:
	static inline uint32 NextUUID = 1; // 0 for Invalid UUID
};
