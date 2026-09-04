#include "StringProperty.h"

#include <memory>

// FString의 크기와 배열 차원을 사용하는 문자열 프로퍼티를 생성한다.
FStringProperty::FStringProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InArrayDimension, EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, sizeof(FString), InArrayDimension, InFlags)
{
}

// 전달받은 원시 메모리에 빈 FString 객체를 생성한다.
void FStringProperty::InitializeElement(void* Value) const
{
	std::construct_at(static_cast<FString*>(Value));
}

// 전달받은 메모리에 존재하는 FString 객체와 문자열 버퍼를 소멸한다.
void FStringProperty::DestroyElement(void* Value) const
{
	std::destroy_at(static_cast<FString*>(Value));
}

// 이미 생성된 목적지 FString에 원본 문자열 값을 복사한다.
void FStringProperty::CopyElement(void* Dst, const void* Src) const
{
	*static_cast<FString*>(Dst) = *static_cast<const FString*>(Src);
}

// FArchive의 FString 연산자를 사용해 문자열 값을 저장하거나 복원한다.
void FStringProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	Ar << *static_cast<FString*>(Value);
}
