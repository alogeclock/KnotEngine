#include "NameProperty.h"

#include <memory>

// FName의 크기와 배열 차원을 사용하는 이름 프로퍼티를 생성한다.
FNameProperty::FNameProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InArrayDimension, EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, sizeof(FName), InArrayDimension, InFlags)
{
}

// 전달받은 원시 메모리에 기본 FName 객체를 생성한다.
void FNameProperty::InitializeElement(void* Value) const
{
	std::construct_at(static_cast<FName*>(Value));
}

// 전달받은 메모리에 존재하는 FName 객체를 소멸한다.
void FNameProperty::DestroyElement(void* Value) const
{
	std::destroy_at(static_cast<FName*>(Value));
}

// 이미 생성된 목적지 FName에 원본 이름 값을 복사한다.
void FNameProperty::CopyElement(void* Dst, const void* Src) const
{
	*static_cast<FName*>(Dst) = *static_cast<const FName*>(Src);
}

// FArchive의 FName 연산자를 사용해 이름 값을 저장하거나 복원한다.
void FNameProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	Ar << *static_cast<FName*>(Value);
}
