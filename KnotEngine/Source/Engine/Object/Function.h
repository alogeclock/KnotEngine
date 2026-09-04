#pragma once

#include "Object/Class.h"

// 리플렉션에 등록된 함수의 호출 방식과 C++ 함수 특성을 나타낸다.
enum class EFunctionFlags : uint32
{
	None = 0,
	Native = 1 << 0,   // 생성된 네이티브 호출 함수가 연결되어 있다.
	Callable = 1 << 1, // 이름 기반 리플렉션 호출을 허용한다.
	Const = 1 << 2,    // 원본 C++ 멤버 함수가 const 함수다.
};

constexpr EFunctionFlags operator|(EFunctionFlags Lhs, EFunctionFlags Rhs)
{
	return static_cast<EFunctionFlags>(static_cast<uint32>(Lhs) | static_cast<uint32>(Rhs));
}

constexpr EFunctionFlags operator&(EFunctionFlags Lhs, EFunctionFlags Rhs)
{
	return static_cast<EFunctionFlags>(static_cast<uint32>(Lhs) & static_cast<uint32>(Rhs));
}

constexpr EFunctionFlags& operator|=(EFunctionFlags& Lhs, EFunctionFlags Rhs)
{
	Lhs = Lhs | Rhs;
	return Lhs;
}

constexpr bool HasFunctionFlag(EFunctionFlags Value, EFunctionFlags Flag)
{
	return (Value & Flag) != EFunctionFlags::None;
}

// 하나의 함수와 매개변수 구조체의 스키마를 함께 표현한다.
// UStruct가 가진 Properties에는 입력, 출력 및 반환값에 해당하는 FProperty가 등록된다.
class UFunction : public UStruct
{
public:
	// 타입이 지워진 Context와 Params를 원래 C++ 객체 및 매개변수 타입으로 변환한 뒤 실제 멤버 함수를 호출하는 중간 함수다.
	using FNativeInvoker = void (*)(UObject* Context, void* Params);

	UFunction(
		FName InName,
		UStruct* InOuterStruct,
		SIZE_T InParamsSize,
		SIZE_T InAlignment,
		EFunctionFlags InFunctionFlags,
		const IStructOps* InParamsOps,
		FNativeInvoker InInvoker);

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	// ConstructParams로 생성되고 호출자가 값을 채운 매개변수 버퍼를 사용해 네이티브 함수를 호출한다.
	void Invoke(UObject* Context, void* Params) const;

	// 타입이 지워진 매개변수 구조체의 생성, 소멸 및 복사를 ParamsOps에 위임한다.
	void ConstructParams(void* Params) const;
	void DestructParams(void* Params) const;
	void CopyParams(void* Dst, const void* Src) const;

	UStruct* GetOuterStruct() const { return OuterStruct; }
	SIZE_T GetParamsSize() const { return GetStructureSize(); }
	EFunctionFlags GetFunctionFlags() const { return FunctionFlags; }
	const FProperty* GetReturnProperty() const;
	bool HasAnyFunctionFlags(EFunctionFlags Flags) const { return (FunctionFlags & Flags) != EFunctionFlags::None; }

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;

	UStruct* OuterStruct = nullptr; // 함수를 선언한 UClass 또는 UStruct.
	EFunctionFlags FunctionFlags = EFunctionFlags::None;
	const IStructOps* ParamsOps = nullptr; // 생성된 매개변수 구조체의 타입별 연산.
	FNativeInvoker Invoker = nullptr;      // 리플렉션 호출을 실제 C++ 함수 호출로 변환하는 생성 코드의 중간 함수.
};

// UFunction 호출에 필요한 정렬된 매개변수 메모리와 그 안의 객체 수명을 범위 단위로 관리한다.
class FScopedFunctionParams
{
public:
	explicit FScopedFunctionParams(const UFunction* InFunction);
	~FScopedFunctionParams();

	// 같은 메모리를 두 번 파괴하거나 해제하지 않도록 소유권 복사를 금지한다.
	FScopedFunctionParams(const FScopedFunctionParams&) = delete;
	FScopedFunctionParams& operator=(const FScopedFunctionParams&) = delete;

	// 호출자가 FProperty 또는 생성된 매개변수 타입을 통해 값을 읽고 쓸 수 있는 버퍼를 반환한다.
	void* GetMemory() { return Memory; }
	const void* GetMemory() const { return Memory; }

private:
	const UFunction* Function = nullptr; // 버퍼의 크기, 정렬과 소멸 연산을 제공한다.
	void* Memory = nullptr;              // 매개변수가 없으면 할당하지 않고 nullptr을 유지한다.
};
