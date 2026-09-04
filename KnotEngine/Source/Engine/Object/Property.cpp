#include "Property.h"

#include "Object/Class.h"

#include <cstddef>

// 프로퍼티의 소유 구조체, 메모리 위치, 원소 크기, 고정 배열 크기와 플래그를 검증해 저장한다.
FProperty::FProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InElementSize, uint32 InArrayDimension, EPropertyFlags InFlags)
	: Name(std::move(InName)), Owner(InOwner), Offset(InOffset), ElementSize(InElementSize), ArrayDimension(InArrayDimension), Flags(InFlags)
{
	check(Owner);
	check(ElementSize > 0);
	check(ArrayDimension > 0);
	check(GetSize() <= Owner->GetStructureSize());
	check(Offset <= Owner->GetStructureSize() - GetSize());
}

// 컨테이너 시작 주소에 프로퍼티 오프셋과 고정 배열 인덱스를 더해 수정 가능한 값 주소를 구한다.
void* FProperty::ContainerPtrToValuePtr(void* Container, uint32 ArrayIndex) const
{
	check(Container);
	check(ArrayIndex < ArrayDimension);
	return static_cast<std::byte*>(Container) + Offset + static_cast<SIZE_T>(ElementSize) * ArrayIndex;
}

// 컨테이너 시작 주소에 프로퍼티 오프셋과 고정 배열 인덱스를 더해 읽기 전용 값 주소를 구한다.
const void* FProperty::ContainerPtrToValuePtr(const void* Container, uint32 ArrayIndex) const
{
	check(Container);
	check(ArrayIndex < ArrayDimension);
	return static_cast<const std::byte*>(Container) + Offset + static_cast<SIZE_T>(ElementSize) * ArrayIndex;
}

// 프로퍼티의 모든 고정 배열 원소를 앞에서부터 생성한다.
void FProperty::InitializeValue(void* Value) const
{
	check(Value);
	for (uint32 Index = 0; Index < ArrayDimension; ++Index)
	{
		InitializeElement(static_cast<std::byte*>(Value) + static_cast<SIZE_T>(ElementSize) * Index);
	}
}

// 프로퍼티의 모든 고정 배열 원소를 생성의 역순으로 소멸한다.
void FProperty::DestroyValue(void* Value) const
{
	check(Value);
	for (uint32 Index = ArrayDimension; Index > 0; --Index)
	{
		DestroyElement(static_cast<std::byte*>(Value) + static_cast<SIZE_T>(ElementSize) * (Index - 1));
	}
}

// 프로퍼티의 모든 고정 배열 원소를 이미 생성된 목적지에 복사한다.
void FProperty::CopyValue(void* Dst, const void* Src) const
{
	check(Dst);
	check(Src);
	for (uint32 Index = 0; Index < ArrayDimension; ++Index)
	{
		const SIZE_T ElementOffset = static_cast<SIZE_T>(ElementSize) * Index;
		CopyElement(static_cast<std::byte*>(Dst) + ElementOffset, static_cast<const std::byte*>(Src) + ElementOffset);
	}
}

// 프로퍼티의 모든 고정 배열 원소를 타입별 직렬화 구현으로 처리한다.
void FProperty::SerializeValue(FArchive& Ar, void* Value) const
{
	check(Value);
	for (uint32 Index = 0; Index < ArrayDimension; ++Index)
	{
		SerializeElement(Ar, static_cast<std::byte*>(Value) + static_cast<SIZE_T>(ElementSize) * Index);
	}
}

// 프로퍼티의 모든 고정 배열 원소가 가진 객체 참조를 타입별 구현으로 방문한다.
void FProperty::VisitReferences(void* Value, FReferenceCollector& Collector) const
{
	check(Value);
	for (uint32 Index = 0; Index < ArrayDimension; ++Index)
	{
		VisitElementReferences(static_cast<std::byte*>(Value) + static_cast<SIZE_T>(ElementSize) * Index, Collector);
	}
}

// 컨테이너에서 프로퍼티 값 주소를 계산해 모든 원소를 직렬화한다.
void FProperty::SerializeInContainer(FArchive& Ar, void* Container) const
{
	SerializeValue(Ar, ContainerPtrToValuePtr(Container));
}

// 컨테이너에서 프로퍼티 값 주소를 계산해 모든 원소의 객체 참조를 방문한다.
void FProperty::VisitReferencesInContainer(void* Container, FReferenceCollector& Collector) const
{
	VisitReferences(ContainerPtrToValuePtr(Container), Collector);
}
