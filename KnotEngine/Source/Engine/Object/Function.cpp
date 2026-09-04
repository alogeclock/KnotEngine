#include "Function.h"

#include "Object/Property.h"

#include <new>

// 함수 소유자, 매개변수 구조체 연산과 네이티브 호출 함수를 검증하고 함수 스키마를 초기화한다.
UFunction::UFunction(
	FName InName,
	UStruct* InOuterStruct,
	SIZE_T InParamsSize,
	SIZE_T InAlignment,
	EFunctionFlags InFunctionFlags,
	const IStructOps* InParamsOps,
	FNativeInvoker InInvoker)
	: UStruct(std::move(InName), InOuterStruct, nullptr, InParamsSize, InAlignment),
	  OuterStruct(InOuterStruct),
	  FunctionFlags(InFunctionFlags),
	  ParamsOps(InParamsOps),
	  Invoker(InInvoker)
{
	check(OuterStruct);
	check((InParamsSize == 0) == (ParamsOps == nullptr));
	check(!HasAnyFunctionFlags(EFunctionFlags::Native) || Invoker);
}

// 타입이 지워진 객체와 매개변수 버퍼를 생성 코드의 중간 함수에 전달해 실제 C++ 함수를 호출한다.
void UFunction::Invoke(UObject* Context, void* Params) const
{
	panic(HasAnyFunctionFlags(EFunctionFlags::Native));
	panic(HasAnyFunctionFlags(EFunctionFlags::Callable));
	panic(Invoker);
	panic(GetParamsSize() == 0 || Params);

	const UClass* OwnerClass = dynamic_cast<const UClass*>(OuterStruct);
	panic(OwnerClass && Context && Context->IsA(OwnerClass));

	Invoker(Context, Params);
}

// 매개변수 구조체가 존재하면 전달받은 원시 메모리에서 그 구조체의 객체 수명을 시작한다.
void UFunction::ConstructParams(void* Params) const
{
	if (ParamsOps)
	{
		check(Params);
		ParamsOps->Construct(Params);
	}
}

// 매개변수 구조체가 존재하면 버퍼 안의 구조체와 그 멤버를 소멸한다.
void UFunction::DestructParams(void* Params) const
{
	if (ParamsOps)
	{
		check(Params);
		ParamsOps->Destruct(Params);
	}
}

// 이미 생성된 목적지 매개변수 구조체에 원본 매개변수 값을 복사한다.
void UFunction::CopyParams(void* Dst, const void* Src) const
{
	if (ParamsOps)
	{
		check(Dst);
		check(Src);
		ParamsOps->Copy(Dst, Src);
	}
}

// 함수에 등록된 프로퍼티 중 ReturnParameter 플래그가 지정된 반환값 프로퍼티를 찾는다.
const FProperty* UFunction::GetReturnProperty() const
{
	TArray<const FProperty*> FunctionProperties;
	GetDeclaredProperties(FunctionProperties);
	for (const FProperty* Property : FunctionProperties)
	{
		if (Property->HasAnyPropertyFlags(EPropertyFlags::ReturnParameter))
		{
			return Property;
		}
	}

	return nullptr;
}

// 함수의 정렬 조건에 맞는 매개변수 버퍼를 할당하고 그 안의 매개변수 구조체를 생성한다.
FScopedFunctionParams::FScopedFunctionParams(const UFunction* InFunction)
	: Function(InFunction)
{
	check(Function);
	if (Function->GetParamsSize() == 0)
	{
		return;
	}

	Memory = ::operator new(Function->GetParamsSize(), std::align_val_t(Function->GetMinAlignment()));
	Function->ConstructParams(Memory);
}

// 매개변수 구조체를 먼저 소멸한 뒤 생성자에서 확보한 정렬된 원시 메모리를 반환한다.
FScopedFunctionParams::~FScopedFunctionParams()
{
	if (!Memory)
	{
		return;
	}

	Function->DestructParams(Memory);
	::operator delete(Memory, std::align_val_t(Function->GetMinAlignment()));
}
