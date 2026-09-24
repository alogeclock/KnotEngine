#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"
#include "Object/ObjectInitializer.h"
#include "Object/Reflection/ReflectionMacros.h"
#include "Object/Reflection/ReflectionRegistry.h"

#include <thread>
#include <type_traits>

class UClass;
class FReferenceCollector;
class FReflectionRegistry;
class FArchive;
class FProperty;
class FObjectInstancingContext;

// 엔진 런타임 객체의 공통 기반 클래스.
// GUObjectArray 등록/해제와 생명주기를 함께하며 UUID 기반 식별.
class ENGINE_API UObject
{
public:
	using ThisClass = UObject;

	UObject();
	virtual ~UObject();

	UObject(const UObject&) = delete;
	UObject& operator=(const UObject&) = delete;
	UObject(UObject&&) = delete;
	UObject& operator=(UObject&&) = delete;

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	UClass* GetClass() const;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

	virtual void PostInitProperties() {}
	virtual void PostDuplicate() {}
	virtual void PostEditProperty(const FProperty& Property) {}
	virtual void PreEditUndo() {}
	virtual void PostEditUndo() {}

	UObject* Duplicate(UObject* NewOuter = nullptr, FName NewName = FName()) const;
	bool IsA(const UClass* Class) const;
	void Serialize(FArchive& Ar);

	bool HasAnyFlags(EObjectFlags InFlags) const { return (Flags & InFlags) != EObjectFlags::None; }
	bool IsTemplate() const { return HasAnyFlags(EObjectFlags::ClassDefaultObject | EObjectFlags::DefaultSubobject) && (!Outer || Outer->IsTemplate()); }
	
	UObject* GetOuter() const { return Outer; }
	const FName& GetObjectName() const { return Name; }
	UObject* GetDefaultSubobject(const FName& Name) const;
	const TArray<UObject*>& GetDefaultSubobjects() const { return DefaultSubobjects; }

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }

protected:
	template <typename T>
	T* CreateDefaultSubobject(FName Name);
	UObject* CreateDefaultSubobject(UClass& SubobjectClass, FName Name);
	void RemoveDefaultSubobject(UObject& Subobject);

private:
	friend class FUObjectManager;
	friend class FReflectionRegistry;
	friend class UClass;

	void SetClass(UClass* InClass);
	static uint32 GenerateUUID();

	static UClass* StaticClassPrivate;
	static uint32 NextUUID;

	UClass* ClassPrivate = nullptr;
	UObject* Outer = nullptr;
	
	FName Name;
	EObjectFlags Flags = EObjectFlags::None;
	TArray<UObject*> DefaultSubobjects;

	uint32 UUID;
	uint32 InternalIndex;
};

extern ENGINE_API TArray<UObject*> GUObjectArray;

// UObject의 생성/파괴 생명주기 관리 및 UUID/InternalIndex 기반 조회를 담당하는 런타임 관리자.
class ENGINE_API FUObjectManager
{
public:
	void Startup();
	void Shutdown();

	bool IsInGameThread() const;

	UObject* NewObject(UClass& Class, UObject* Outer = nullptr, FName Name = FName(), UObject* Template = nullptr,
	                 EObjectFlags Flags = EObjectFlags::None, FObjectInstancingContext* ExistingContext = nullptr);
	UObject* DuplicateObject(const UObject& Source, UObject* NewOuter = nullptr, FName NewName = FName());

	void Destroy(UObject* Object)
	{
		check(IsInGameThread());
		if (!Object)
		{
			return;
		}

		delete Object;
	}

	UObject* FindByUUID(uint32 UUID) const
	{
		check(IsInGameThread());
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
		check(IsInGameThread());
		if (Index >= GUObjectArray.size())
		{
			return nullptr;
		}

		return GUObjectArray[Index];
	}

private:
	std::thread::id GameThreadId;
};

extern ENGINE_API FUObjectManager GUObjectManager;

template <typename T>
T* NewObject(UObject* Outer = nullptr, FName Name = FName(), UObject* Template = nullptr, EObjectFlags Flags = EObjectFlags::None,
             FObjectInstancingContext* ExistingContext = nullptr)
{
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");
	static_assert(std::is_same_v<typename T::ThisClass, T>, "UObject subclasses require GENERATED_CLASS");
	panic(GReflectionRegistry);
	return static_cast<T*>(GUObjectManager.NewObject(*T::StaticClass(), Outer, std::move(Name), Template, Flags, ExistingContext));
}

#include "Object/ObjectPtr.h"

template <typename T>
T* UObject::CreateDefaultSubobject(FName Name)
{
	static_assert(std::is_base_of_v<UObject, T>);
	return static_cast<T*>(CreateDefaultSubobject(*T::StaticClass(), std::move(Name)));
}
