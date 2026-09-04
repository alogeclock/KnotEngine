#include "SoftObjectProperty.h"

#include "Object/Class.h"

// 참조할 클래스와 TSoftObjectPtr 저장 형식의 연산을 사용하는 소프트 객체 참조 프로퍼티를 생성한다.
FSoftObjectProperty::FSoftObjectProperty(
	FName InName,
	const UStruct* InOwner,
	uint32 InOffset,
	const UClass* InPropertyClass,
	const ISoftObjectPtrOps* InSoftObjectPtrOps,
	uint32 InArrayDimension,
	EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, InSoftObjectPtrOps ? InSoftObjectPtrOps->GetValueSize() : 0, InArrayDimension, InFlags),
	  PropertyClass(InPropertyClass), SoftObjectPtrOps(InSoftObjectPtrOps)
{
	check(PropertyClass);
	check(SoftObjectPtrOps);
}

// 전달받은 원시 메모리에 비어 있는 TSoftObjectPtr 값을 생성한다.
void FSoftObjectProperty::InitializeElement(void* Value) const
{
	SoftObjectPtrOps->InitializeValue(Value);
}

// 전달받은 메모리에 존재하는 TSoftObjectPtr 값의 수명을 끝낸다.
void FSoftObjectProperty::DestroyElement(void* Value) const
{
	SoftObjectPtrOps->DestroyValue(Value);
}

// 이미 생성된 목적지 TSoftObjectPtr에 물리 에셋 경로와 캐시를 복사한다.
void FSoftObjectProperty::CopyElement(void* Dst, const void* Src) const
{
	SoftObjectPtrOps->CopyValue(Dst, Src);
}

// 객체를 강하게 참조하지 않고 물리 에셋 경로만 저장하거나 복원한다.
void FSoftObjectProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	FString Path = Ar.IsSaving() ? SoftObjectPtrOps->GetPath(Value) : FString();
	Ar << Path;
	if (Ar.IsLoading())
	{
		SoftObjectPtrOps->SetPath(Value, std::move(Path));
	}
}
