#include "StructProperty.h"

#include "Object/Class.h"

#include <limits>

// 값 타입 스키마의 메모리 크기와 배열 차원을 사용하는 구조체 프로퍼티를 생성한다.
FStructProperty::FStructProperty(
	FName InName,
	const UStruct* InOwner,
	uint32 InOffset,
	const UScriptStruct* InStruct,
	uint32 InArrayDimension,
	EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, InStruct ? static_cast<uint32>(InStruct->GetStructureSize()) : 0,
		InArrayDimension, InFlags),
	  Struct(InStruct)
{
	panic(Struct);
	check(Struct->GetStructureSize() <= std::numeric_limits<uint32>::max());
}

// UScriptStruct에 등록된 연산으로 값 타입 객체를 생성한다.
void FStructProperty::InitializeElement(void* Value) const
{
	Struct->Construct(Value);
}

// UScriptStruct에 등록된 연산으로 값 타입 객체를 소멸한다.
void FStructProperty::DestroyElement(void* Value) const
{
	Struct->Destruct(Value);
}

// UScriptStruct에 등록된 연산으로 이미 생성된 목적지에 원본 구조체 값을 복사한다.
void FStructProperty::CopyElement(void* Dst, const void* Src) const
{
	Struct->Copy(Dst, Src);
}

// 상속을 포함한 구조체 프로퍼티를 순회하며 Transient가 아닌 멤버를 직렬화한다.
void FStructProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	TArray<const FProperty*> StructProperties;
	Struct->GetAllProperties(StructProperties);
	for (const FProperty* Property : StructProperties)
	{
		if (!Property->HasAnyPropertyFlags(EPropertyFlags::Transient))
		{
			Property->SerializeInContainer(Ar, Value);
		}
	}
}

// 상속을 포함한 구조체 프로퍼티를 순회하며 모든 멤버의 객체 참조를 방문한다.
void FStructProperty::VisitElementReferences(void* Value, FReferenceCollector& Collector) const
{
	TArray<const FProperty*> StructProperties;
	Struct->GetAllProperties(StructProperties);
	for (const FProperty* Property : StructProperties)
	{
		Property->VisitReferencesInContainer(Value, Collector);
	}
}
