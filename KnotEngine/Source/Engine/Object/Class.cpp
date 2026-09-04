#include "Class.h"

#include "Object/Function.h"
#include "Object/Property.h"

// 필드의 이름과 자신을 포함하는 상위 필드를 저장한다.
UField::UField(FName InName, UField* InOwner)
	: Name(std::move(InName)), Owner(InOwner)
{
}

// 구조체의 상속 관계와 실제 C++ 메모리 크기 및 정렬 정보를 저장한다.
UStruct::UStruct(FName InName, UField* InOwner, const UStruct* InSuperStruct, SIZE_T InStructureSize, SIZE_T InMinAlignment)
	: UField(std::move(InName), InOwner), SuperStruct(InSuperStruct), StructureSize(InStructureSize), MinAlignment(InMinAlignment)
{
	check(MinAlignment > 0);
}

// 전방 선언된 FProperty의 소유 컨테이너가 완전한 타입인 위치에서 소멸자를 정의한다.
UStruct::~UStruct() = default;

// 소유자가 일치하고 이름이 중복되지 않는 프로퍼티의 소유권을 구조체에 등록한다.
FProperty* UStruct::AddProperty(std::unique_ptr<FProperty> Property)
{
	check(Property);
	check(Property->GetOwner() == this);
	panic(!FindProperty(Property->GetFName()));
	panic(Property->GetSize() <= StructureSize);
	panic(Property->GetOffset() <= StructureSize - Property->GetSize());

	FProperty* Result = Property.get();
	Properties.push_back(std::move(Property));
	return Result;
}

// 현재 구조체에서 프로퍼티를 찾고 없으면 상위 구조체를 따라 검색한다.
const FProperty* UStruct::FindProperty(const FName& PropertyName) const
{
	for (const std::unique_ptr<FProperty>& Property : Properties)
	{
		if (Property->GetFName() == PropertyName)
		{
			return Property.get();
		}
	}

	return SuperStruct ? SuperStruct->FindProperty(PropertyName) : nullptr;
}

// 현재 구조체에 직접 선언된 프로퍼티만 출력 배열 뒤에 추가한다.
void UStruct::GetDeclaredProperties(TArray<const FProperty*>& OutProperties) const
{
	for (const std::unique_ptr<FProperty>& Property : Properties)
	{
		OutProperties.push_back(Property.get());
	}
}

// 상위 구조체부터 현재 구조체 순서로 모든 프로퍼티를 출력 배열 뒤에 추가한다.
void UStruct::GetAllProperties(TArray<const FProperty*>& OutProperties) const
{
	if (SuperStruct)
	{
		SuperStruct->GetAllProperties(OutProperties);
	}

	GetDeclaredProperties(OutProperties);
}

// NoEdit은 에디터 열거에서만 제외한다. 직렬화와 참조 수집은 전체 프로퍼티를 사용한다.
void UStruct::GetEditorProperties(TArray<const FProperty*>& OutProperties) const
{
	TArray<const FProperty*> AllProperties;
	GetAllProperties(AllProperties);
	for (const FProperty* Property : AllProperties)
	{
		if (!Property->HasAnyPropertyFlags(EPropertyFlags::NoEdit))
		{
			OutProperties.push_back(Property);
		}
	}
}

// 객체와 중첩 값 타입이 같은 Transient 선택 규칙으로 필드를 저장하고 복원한다.
void UStruct::SerializeProperties(FArchive& Ar, void* Container) const
{
	TArray<const FProperty*> AllProperties;
	GetAllProperties(AllProperties);
	for (const FProperty* Property : AllProperties)
	{
		Property->SerializeInContainer(Ar, Container);
	}
}

// 클래스의 상속 정보, 메모리 크기, 플래그와 객체 생성 함수를 저장한다.
UClass::UClass(FName InName, UClass* InSuperClass, SIZE_T InClassSize, SIZE_T InMinAlignment, EClassFlags InClassFlags, FCreateObjectFunc InCreateFunc)
	: UStruct(std::move(InName), nullptr, InSuperClass, InClassSize, InMinAlignment), ClassFlags(InClassFlags), CreateFunc(InCreateFunc)
{
	check(InClassSize > 0);
}

// 전방 선언된 UFunction의 소유 컨테이너가 완전한 타입인 위치에서 소멸자를 정의한다.
UClass::~UClass() = default;

// 자신부터 상위 클래스까지 순회하며 지정한 클래스와의 상속 관계를 확인한다.
bool UClass::IsChildOf(const UClass* Other) const
{
	if (!Other)
	{
		return false;
	}

	for (const UClass* Class = this; Class; Class = Class->GetSuperClass())
	{
		if (Class == Other)
		{
			return true;
		}
	}

	return false;
}

// 추상 클래스가 아닌지 확인한 뒤 등록된 생성 함수로 UObject 인스턴스를 만든다.
UObject* UClass::CreateObject() const
{
	panic(CreateFunc && !HasAnyClassFlags(EClassFlags::Abstract));

	UObject* Object = CreateFunc(const_cast<UClass*>(this));
	panic(Object);
	panic(Object->GetClass() == this);
	return Object;
}

// 소유자가 일치하고 이름이 중복되지 않는 함수의 소유권을 클래스에 등록한다.
UFunction* UClass::AddFunction(std::unique_ptr<UFunction> Function)
{
	check(Function);
	check(Function->GetOuterStruct() == this);
	panic(!FindFunction(Function->GetFName()));

	Function->SetClass(UFunction::StaticClass());

	UFunction* Result = Function.get();
	Functions.push_back(std::move(Function));
	return Result;
}

// 현재 클래스에서 함수를 찾고 없으면 상위 클래스를 따라 검색한다.
const UFunction* UClass::FindFunction(const FName& FunctionName) const
{
	for (const std::unique_ptr<UFunction>& Function : Functions)
	{
		if (Function->GetFName() == FunctionName)
		{
			return Function.get();
		}
	}

	const UClass* SuperClass = GetSuperClass();
	return SuperClass ? SuperClass->FindFunction(FunctionName) : nullptr;
}

// 상위 클래스부터 현재 클래스 순서로 모든 함수를 출력 배열 뒤에 추가한다.
void UClass::GetAllFunctions(TArray<const UFunction*>& OutFunctions) const
{
	if (const UClass* SuperClass = GetSuperClass())
	{
		SuperClass->GetAllFunctions(OutFunctions);
	}

	for (const std::unique_ptr<UFunction>& Function : Functions)
	{
		OutFunctions.push_back(Function.get());
	}
}

// 값 타입 구조체의 메모리 정보와 타입이 지워진 생성, 소멸 및 복사 연산을 저장한다.
UScriptStruct::UScriptStruct(
	FName InName,
	SIZE_T InSize,
	SIZE_T InAlignment,
	const IStructOps* InStructOps,
	UField* InOwner,
	const UScriptStruct* InSuperStruct)
	: UStruct(std::move(InName), InOwner, InSuperStruct, InSize, InAlignment), StructOps(InStructOps)
{
	check(InSize > 0);
	check(StructOps);
}

// 등록된 구조체 연산을 사용해 원시 메모리에서 값 타입 객체의 수명을 시작한다.
void UScriptStruct::Construct(void* Ptr) const
{
	check(Ptr);
	StructOps->Construct(Ptr);
}

// 등록된 구조체 연산을 사용해 원시 메모리에 존재하는 값 타입 객체를 소멸한다.
void UScriptStruct::Destruct(void* Ptr) const
{
	check(Ptr);
	StructOps->Destruct(Ptr);
}

// 등록된 구조체 연산을 사용해 이미 생성된 목적지 객체에 원본 값을 복사한다.
void UScriptStruct::Copy(void* Dst, const void* Src) const
{
	check(Dst);
	check(Src);
	StructOps->Copy(Dst, Src);
}

// 열거형의 바이트 크기와 이름, 표시 이름 및 정수값 목록을 저장한다.
UEnum::UEnum(FName InName, uint8 InSize, TArray<FEnumValue> InValues, UField* InOwner)
	: UField(std::move(InName), InOwner), Size(InSize), Values(std::move(InValues))
{
	check(Size == 1 || Size == 2 || Size == 4 || Size == 8);
}

// 등록된 열거형 값 중 지정한 이름과 일치하는 항목을 찾는다.
const FEnumValue* UEnum::FindValueByName(const FName& Name) const
{
	for (const FEnumValue& Value : Values)
	{
		if (Value.Name == Name)
		{
			return &Value;
		}
	}

	return nullptr;
}
