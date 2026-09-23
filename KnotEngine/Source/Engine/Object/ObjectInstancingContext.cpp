#include "Object/ObjectInstancingContext.h"

#include "Core/Assert.h"

// 원본 객체 하나에 정확히 하나의 목적지 객체를 대응시킨다.
void FObjectInstancingContext::Add(const UObject& Source, UObject& Destination)
{
	check(!ObjectMap.contains(&Source));
	ObjectMap.emplace(&Source, &Destination);
	Objects.emplace_back(&Source, &Destination);
}

// 원본 객체에 대응하는 Instance가 있으면 반환한다.
UObject* FObjectInstancingContext::Find(const UObject* Source) const
{
	if (!Source)
	{
		return nullptr;
	}
	const auto Iterator = ObjectMap.find(Source);
	return Iterator != ObjectMap.end() ? Iterator->second : nullptr;
}
