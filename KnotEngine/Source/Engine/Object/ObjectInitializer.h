#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"
#include "Core/Name.h"

class UClass;
class UObject;
class FObjectInstancingContext;

enum class EObjectFlags : uint32
{
	None = 0,
	ClassDefaultObject = 1 << 0,
	DefaultSubobject = 1 << 1,
};

constexpr EObjectFlags operator|(EObjectFlags Lhs, EObjectFlags Rhs)
{
	return static_cast<EObjectFlags>(static_cast<uint32>(Lhs) | static_cast<uint32>(Rhs));
}

constexpr EObjectFlags operator&(EObjectFlags Lhs, EObjectFlags Rhs)
{
	return static_cast<EObjectFlags>(static_cast<uint32>(Lhs) & static_cast<uint32>(Rhs));
}

// UObject 생성 중 실제 클래스, 소유 객체, Template과 플래그를 생성자에 제공한다.
class ENGINE_API FObjectInitializer final
{
public:
	FObjectInitializer(UClass& InClass, UObject* InOuter, FName InName, UObject* InTemplate,
		EObjectFlags InFlags, FObjectInstancingContext* InInstancingContext = nullptr);
	~FObjectInitializer();

	FObjectInitializer(const FObjectInitializer&) = delete;
	FObjectInitializer& operator=(const FObjectInitializer&) = delete;

	static FObjectInitializer* GetCurrent();
	static FObjectInitializer& Get();

	UClass& GetClass() const { return Class; }
	UObject* GetOuter() const { return Outer; }
	UObject* GetTemplate() const { return Template; }
	const FName& GetName() const { return Name; }
	EObjectFlags GetFlags() const { return Flags; }
	FObjectInstancingContext* GetInstancingContext() const { return InstancingContext; }

private:
	static FObjectInitializer*& GetCurrentStorage();

	FObjectInitializer* Previous = nullptr;

	UClass& Class;
	UObject* Outer = nullptr;
	UObject* Template = nullptr;
	FName Name;
	EObjectFlags Flags = EObjectFlags::None;

	FObjectInstancingContext* InstancingContext = nullptr;
};
