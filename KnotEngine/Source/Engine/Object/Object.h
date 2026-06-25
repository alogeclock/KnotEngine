#pragma once

#include "Class.h"

// 엔진 런타임 객체의 공통 기반 클래스. 
// GUObjectArray 등록/해제와 생명주기를 함께하며 UUID 기반 식별.
class UObject
{
public:
	UObject();
	virtual ~UObject();

	virtual UClass* GetClass() const = 0;

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; } 

private:
	uint32 UUID;
	uint32 InternalIndex;
};

extern TArray<UObject*> GUObjectArray;

// UObject의 생성/파괴 생명주기 관리 및 UUID/InternalIndex 기반 조회를 담당하는 런타임 관리자.
class FUObjectManager
{
public:
	template<typename T> 
	T* Create()
	{
		static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
		T* Object = new T();
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

	UObject* FindByUUID(uint32 UUID)
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

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size())
		{
			return nullptr;
		}
		
		return GUObjectArray[Index];
	}
};

extern FUObjectManager GUObjectManager;
