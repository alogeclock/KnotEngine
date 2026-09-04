#include "EnumProperty.h"

#include "Object/Class.h"

#include <cstring>

// 지정한 열거형 스키마의 바이트 크기를 사용하는 프로퍼티를 생성한다.
FEnumProperty::FEnumProperty(FName InName, const UStruct* InOwner, uint32 InOffset, const UEnum* InEnum, uint32 InArrayDimension, EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, InEnum ? InEnum->GetSize() : 0, InArrayDimension, InFlags), Enum(InEnum)
{
	panic(Enum);
}

// 열거형 저장 공간의 모든 바이트를 0으로 초기화한다.
void FEnumProperty::InitializeElement(void* Value) const
{
	std::memset(Value, 0, Enum->GetSize());
}

// 열거형의 실제 바이트 크기만큼 원본 값을 목적지에 복사한다.
void FEnumProperty::CopyElement(void* Dst, const void* Src) const
{
	std::memcpy(Dst, Src, Enum->GetSize());
}

// 열거형 값을 스키마에 등록된 바이트 크기 그대로 직렬화한다.
void FEnumProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	Ar.Serialize(Value, Enum->GetSize());
}
