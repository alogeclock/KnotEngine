#pragma once

#include "Core/CoreTypes.h"

#include <type_traits>

class UClass;
class FReferenceCollector;
class FReflectionRegistry;

// 엔진 런타임 객체의 공통 기반 클래스.
// GUObjectArray 등록/해제와 생명주기를 함께하며 UUID 기반 식별.
class UObject
{
public:
	UObject();
	virtual ~UObject();

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	UClass* GetClass() const;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
	bool IsA(const UClass* Class) const;

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }

private:
	friend class FUObjectManager;
	friend class FReflectionRegistry;
	friend class UClass;

	void SetClass(UClass* InClass);

	static UClass* StaticClassPrivate;
	UClass* ClassPrivate = nullptr;

	uint32 UUID;
	uint32 InternalIndex;
};

extern TArray<UObject*> GUObjectArray;

// UObject의 생성/파괴 생명주기 관리 및 UUID/InternalIndex 기반 조회를 담당하는 런타임 관리자.
class FUObjectManager
{
public:
	template <typename T>
	T* Create()
	{
		return Create<T>(T::StaticClass());
	}

	template <typename T>
	T* Create(UClass* InClass)
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
		panic(InClass);

		T* Object = new T();
		Object->SetClass(InClass);

		return Object;
	}

	void Destroy(UObject* Object)
	{
		if (!Object)
		{
			return;
		}

		delete Object;
	}

	UObject* FindByUUID(uint32 UUID) const
	{
		for (auto* Obj : GUObjectArray)
		{
			if (Obj && Obj->GetUUID() == UUID)
			{
				return Obj;
			}
		}

		return nullptr;
	}

	UObject* FindByIndex(uint32 Index) const
	{
		if (Index >= GUObjectArray.size())
		{
			return nullptr;
		}

		return GUObjectArray[Index];
	}

};

extern FUObjectManager GUObjectManager;

#include "Object/ObjectPtr.h"
