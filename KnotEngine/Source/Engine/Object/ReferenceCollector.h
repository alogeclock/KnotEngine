#pragma once

#include "Core/CoreTypes.h"

class UObject;

template <typename T>
class TObjectPtr;

// 루트에서 도달 가능한 UObject 집합을 구성하는 mark 수집기.
class FReferenceCollector
{
public:
	void CollectReferences(UObject* RootObject);
	const TSet<UObject*>& GetReachableObjects() const { return ReachableObjects; }

	void AddReferencedObject(UObject* Object);

	template <typename T>
	void AddReferencedObject(const TObjectPtr<T>& Object)
	{
		AddReferencedObject(Object.Get());
	}

	template <typename T>
	void AddReferencedObjects(const TArray<TObjectPtr<T>>& Objects)
	{
		for (const TObjectPtr<T>& Object : Objects)
		{
			AddReferencedObject(Object);
		}
	}

private:
	void VisitObject(UObject* Object);

	TSet<UObject*> ReachableObjects;
};
