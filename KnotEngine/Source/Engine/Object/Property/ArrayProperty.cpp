#include "ArrayProperty.h"

#include <limits>

// 배열 컨테이너의 타입별 연산과 원소 프로퍼티의 소유권을 등록한다.
FArrayProperty::FArrayProperty(
	FName InName,
	const UStruct* InOwner,
	uint32 InOffset,
	uint32 InArraySize,
	const FArrayOps* InArrayOps,
	std::unique_ptr<FProperty> InInner,
	EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, InArraySize, 1, InFlags), ArrayOps(InArrayOps), Inner(std::move(InInner))
{
	check(ArrayOps);
	check(ArrayOps->Construct && ArrayOps->Destruct && ArrayOps->Copy && ArrayOps->Num && ArrayOps->Resize && ArrayOps->GetElement);
	check(Inner);
}

// 등록된 배열 연산으로 빈 배열 컨테이너를 생성한다.
void FArrayProperty::InitializeElement(void* Value) const
{
	ArrayOps->Construct(Value);
}

// 등록된 배열 연산으로 배열 원소와 컨테이너를 소멸한다.
void FArrayProperty::DestroyElement(void* Value) const
{
	ArrayOps->Destruct(Value);
}

// 등록된 배열 연산으로 이미 생성된 목적지 배열에 원본 배열을 복사한다.
void FArrayProperty::CopyElement(void* Dst, const void* Src) const
{
	ArrayOps->Copy(Dst, Src);
}

// 배열 길이를 저장하거나 복원한 뒤 내부 프로퍼티를 사용해 각 원소를 직렬화한다.
void FArrayProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	const SIZE_T ValueCount = ArrayOps->Num(Value);
	check(ValueCount <= std::numeric_limits<uint32>::max());
	uint32 SerializedCount = static_cast<uint32>(ValueCount);
	Ar << SerializedCount;

	if (Ar.IsLoading())
	{
		ArrayOps->Resize(Value, SerializedCount);
	}

	for (uint32 Index = 0; Index < SerializedCount; ++Index)
	{
		Inner->SerializeValue(Ar, ArrayOps->GetElement(Value, Index));
	}
}

// 내부 프로퍼티를 사용해 배열의 모든 원소가 가진 객체 참조를 방문한다.
void FArrayProperty::VisitElementReferences(void* Value, FReferenceCollector& Collector) const
{
	const SIZE_T ValueCount = ArrayOps->Num(Value);
	for (SIZE_T Index = 0; Index < ValueCount; ++Index)
	{
		Inner->VisitReferences(ArrayOps->GetElement(Value, Index), Collector);
	}
}
