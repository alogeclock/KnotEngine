#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

class UStaticMesh;

// .kasset의 형식 검증, Payload 역직렬화와 UObject Asset 생성을 담당한다.
class ENGINE_API FAssetBinaryLoader final
{
public:
	UStaticMesh* LoadStaticMesh(const FString& AssetPath) const;
};
