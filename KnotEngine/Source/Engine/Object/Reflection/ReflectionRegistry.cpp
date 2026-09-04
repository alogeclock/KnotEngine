#include "ReflectionRegistry.h"

#include "Object/Class.h"
#include "Object/Function.h"

FReflectionRegistry* GReflectionRegistry = nullptr;

UClass* UObject::StaticClassPrivate = nullptr;
UClass* UField::StaticClassPrivate = nullptr;
UClass* UStruct::StaticClassPrivate = nullptr;
UClass* UClass::StaticClassPrivate = nullptr;
UClass* UScriptStruct::StaticClassPrivate = nullptr;
UClass* UFunction::StaticClassPrivate = nullptr;
UClass* UEnum::StaticClassPrivate = nullptr;

// 엔진 루프가 소유할 비활성 레지스트리 객체를 생성한다.
FReflectionRegistry::FReflectionRegistry() = default;

// 소멸 전에 전역 활성 레지스트리 등록이 해제됐는지 확인한다.
FReflectionRegistry::~FReflectionRegistry()
{
	check(GReflectionRegistry != this);
}

// 현재 인스턴스를 엔진 전체에서 사용할 수 있는 유일한 활성 레지스트리로 등록한다.
void FReflectionRegistry::Startup()
{
	panic(GReflectionRegistry == nullptr);
	panic(UClass::StaticClassPrivate == nullptr);
	auto UObjectClass = std::make_unique<UClass>(
		FName("UObject"), nullptr, sizeof(UObject), alignof(UObject), EClassFlags::None,
		[](UClass* Class) -> UObject* { panic(Class == UObject::StaticClass()); return GUObjectManager.Create<UObject>(); });
	auto UFieldClass = std::make_unique<UClass>(
		FName("UField"), UObjectClass.get(), sizeof(UField), alignof(UField), EClassFlags::None);
	auto UStructClass = std::make_unique<UClass>(
		FName("UStruct"), UFieldClass.get(), sizeof(UStruct), alignof(UStruct), EClassFlags::None);
	auto UClassClass = std::make_unique<UClass>(
		FName("UClass"), UStructClass.get(), sizeof(UClass), alignof(UClass), EClassFlags::None);
	auto UScriptStructClass = std::make_unique<UClass>(
		FName("UScriptStruct"), UStructClass.get(), sizeof(UScriptStruct), alignof(UScriptStruct), EClassFlags::None);
	auto UFunctionClass = std::make_unique<UClass>(
		FName("UFunction"), UStructClass.get(), sizeof(UFunction), alignof(UFunction), EClassFlags::None);
	auto UEnumClass = std::make_unique<UClass>(
		FName("UEnum"), UFieldClass.get(), sizeof(UEnum), alignof(UEnum), EClassFlags::None);
	UObject::StaticClassPrivate = UObjectClass.get();
	UField::StaticClassPrivate = UFieldClass.get();
	UStruct::StaticClassPrivate = UStructClass.get();
	UClass::StaticClassPrivate = UClassClass.get();
	UScriptStruct::StaticClassPrivate = UScriptStructClass.get();
	UFunction::StaticClassPrivate = UFunctionClass.get();
	UEnum::StaticClassPrivate = UEnumClass.get();
	RegisterClass(std::move(UObjectClass));
	RegisterClass(std::move(UFieldClass));
	RegisterClass(std::move(UStructClass));
	RegisterClass(std::move(UClassClass));
	RegisterClass(std::move(UScriptStructClass));
	RegisterClass(std::move(UFunctionClass));
	RegisterClass(std::move(UEnumClass));
	panic(UClass::StaticClass()->GetClass() == UClass::StaticClass());
	RegisterStaticClass();
	GReflectionRegistry = this;
}

// 등록된 모든 필드를 파괴하고 전역 활성 레지스트리 포인터를 해제한다.
void FReflectionRegistry::Shutdown()
{
	panic(GReflectionRegistry == this);
	GReflectionRegistry = nullptr;
	ResetStaticClass();
	UObject::StaticClassPrivate = nullptr;
	UField::StaticClassPrivate = nullptr;
	UStruct::StaticClassPrivate = nullptr;
	UClass::StaticClassPrivate = nullptr;
	UScriptStruct::StaticClassPrivate = nullptr;
	UFunction::StaticClassPrivate = nullptr;
	UEnum::StaticClassPrivate = nullptr;
	FieldsByName.clear();
	Fields.clear();
}

// 클래스 스키마의 소유권을 공통 필드 저장소에 등록하고 UClass 포인터를 반환한다.
UClass* FReflectionRegistry::RegisterClass(std::unique_ptr<UClass> Class)
{
	panic(Class);
	Class->SetClass(UClass::StaticClass());
	return static_cast<UClass*>(RegisterField(std::move(Class)));
}

// 값 타입 구조체 스키마의 소유권을 공통 필드 저장소에 등록한다.
UScriptStruct* FReflectionRegistry::RegisterScriptStruct(std::unique_ptr<UScriptStruct> Struct)
{
	panic(Struct);
	Struct->SetClass(UScriptStruct::StaticClass());
	return static_cast<UScriptStruct*>(RegisterField(std::move(Struct)));
}

// 열거형 스키마의 소유권을 공통 필드 저장소에 등록한다.
UEnum* FReflectionRegistry::RegisterEnum(std::unique_ptr<UEnum> Enum)
{
	panic(Enum);
	Enum->SetClass(UEnum::StaticClass());
	return static_cast<UEnum*>(RegisterField(std::move(Enum)));
}

// 활성 레지스트리의 이름 검색 맵에서 필드를 찾는다.
UField* FReflectionRegistry::FindField(const FName& Name) const
{
	panic(GReflectionRegistry == this);
	const auto Iterator = FieldsByName.find(Name);
	return Iterator != FieldsByName.end() ? Iterator->second : nullptr;
}

// 이름이 일치하는 필드가 클래스 스키마인지 확인해 반환한다.
UClass* FReflectionRegistry::FindClass(const FName& Name) const
{
	return dynamic_cast<UClass*>(FindField(Name));
}

// 이름이 일치하는 필드가 값 타입 구조체 스키마인지 확인해 반환한다.
UScriptStruct* FReflectionRegistry::FindScriptStruct(const FName& Name) const
{
	return dynamic_cast<UScriptStruct*>(FindField(Name));
}

// 이름이 일치하는 필드가 열거형 스키마인지 확인해 반환한다.
UEnum* FReflectionRegistry::FindEnum(const FName& Name) const
{
	return dynamic_cast<UEnum*>(FindField(Name));
}

// 유효하고 중복되지 않는 필드를 이름 검색 맵과 소유 배열에 함께 등록한다.
UField* FReflectionRegistry::RegisterField(std::unique_ptr<UField> Field)
{
	panic(GReflectionRegistry == nullptr);
	panic(Field);

	const FName Name = Field->GetFName();
	panic(!Name.IsNone());
	panic(!FieldsByName.contains(Name));

	UField* Result = Field.get();
	FieldsByName.emplace(Name, Result);
	Fields.push_back(std::move(Field));
	return Result;
}
