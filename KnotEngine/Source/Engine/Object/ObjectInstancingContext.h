#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

class UObject;

// CDO 생성과 객체 복제에서 원본 UObject 참조를 대응하는 Instance 참조로 치환한다.
class ENGINE_API FObjectInstancingContext final
{
public:
	void Add(const UObject& Source, UObject& Destination);
	UObject* Find(const UObject* Source) const;
	const TArray<std::pair<const UObject*, UObject*>>& GetObjects() const { return Objects; }

private:
	TMap<const UObject*, UObject*> ObjectMap;
	TArray<std::pair<const UObject*, UObject*>> Objects;
};
