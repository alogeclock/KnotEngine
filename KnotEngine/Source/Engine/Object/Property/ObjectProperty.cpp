#include "ObjectProperty.h"

#include "Object/Class.h"
#include "Object/Object.h"

// 참조할 클래스와 실제 포인터 저장 형식의 연산을 사용하는 강한 객체 참조 프로퍼티를 생성한다.
FObjectProperty::FObjectProperty(
	FName InName,
	const UStruct* InOwner,
	uint32 InOffset,
	const UClass* InPropertyClass,
	const IObjectPtrOps* InObjectPtrOps,
	uint32 InArrayDimension,
	EPropertyFlags InFlags)
	: FProperty(std::move(InName), InOwner, InOffset, InObjectPtrOps ? InObjectPtrOps->GetValueSize() : 0, InArrayDimension,
		InFlags | EPropertyFlags::ObjectReference), PropertyClass(InPropertyClass), ObjectPtrOps(InObjectPtrOps)
{
	check(PropertyClass);
	check(ObjectPtrOps);
}

// 포인터 저장 형식에 맞춰 null 객체 참조 값을 생성한다.
void FObjectProperty::InitializeElement(void* Value) const
{
	ObjectPtrOps->InitializeValue(Value);
}

// 포인터 저장 형식에 맞춰 객체 참조 값의 수명을 끝낸다.
void FObjectProperty::DestroyElement(void* Value) const
{
	ObjectPtrOps->DestroyValue(Value);
}

// 포인터 저장 형식에 맞춰 이미 생성된 목적지에 객체 참조를 복사한다.
void FObjectProperty::CopyElement(void* Dst, const void* Src) const
{
	ObjectPtrOps->CopyValue(Dst, Src);
}

// 강한 객체 참조를 현재 실행의 UUID로 저장하고 이미 등록된 UObject 주소로 복원한다.
void FObjectProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	uint32 ObjectUUID = 0;
	if (Ar.IsSaving())
	{
		if (UObject* Object = ObjectPtrOps->GetObject(Value))
		{
			ObjectUUID = Object->GetUUID();
		}
	}

	Ar << ObjectUUID;
	if (Ar.IsLoading())
	{
		UObject* Object = ObjectUUID != 0 ? GUObjectManager.FindByUUID(ObjectUUID) : nullptr;
		ObjectPtrOps->SetObject(Value, Object && Object->IsA(PropertyClass) ? Object : nullptr);
	}
}

// 포인터 저장 형식에서 UObject를 꺼내 강한 참조 수집기에 전달한다.
void FObjectProperty::VisitElementReferences(void* Value, FReferenceCollector& Collector) const
{
	ObjectPtrOps->VisitReference(Value, Collector);
}
