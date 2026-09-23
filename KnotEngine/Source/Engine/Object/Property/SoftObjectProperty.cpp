#include "SoftObjectProperty.h"

#include "Core/Archive/StructuredArchive.h"
#include "Object/Reflection/Class.h"

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

// 이미 생성된 목적지 TSoftObjectPtr에 영속 Asset ID와 그 ID를 해석한 비소유 캐시를 복사한다.
void FSoftObjectProperty::CopyElement(void* Dst, const void* Src) const
{
	SoftObjectPtrOps->CopyValue(Dst, Src);
}

// 객체를 로드하거나 강하게 참조하지 않고 소프트 참조의 유일한 영속 값인 Asset ID만 저장하거나 복원한다.
void FSoftObjectProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	FAssetId AssetId = Ar.IsSaving() ? SoftObjectPtrOps->GetAssetId(Value) : FAssetId();
	Ar << AssetId;
	if (Ar.IsLoading())
	{
		SoftObjectPtrOps->SetAssetId(Value, AssetId);
	}
}

// 소프트 참조의 Asset ID를 null 또는 32자리 16진수 문자열로 저장하고 복원한다.
void FSoftObjectProperty::SerializeElement(FStructuredArchiveSlot Slot, void* Value) const
{
	if (Slot.IsSaving())
	{
		const FAssetId& AssetId = SoftObjectPtrOps->GetAssetId(Value);
		if (!AssetId.IsValid())
		{
			Slot.SetNull();
			return;
		}
		FString Text = AssetId.ToString();
		Slot << Text;
		return;
	}

	if (Slot.IsNull())
	{
		SoftObjectPtrOps->SetAssetId(Value, {});
		return;
	}

	FString Text;
	Slot << Text;
	FAssetId AssetId;
	if (!FAssetId::TryParse(Text, AssetId))
	{
		Slot.GetArchive().SetError();
		return;
	}
	SoftObjectPtrOps->SetAssetId(Value, AssetId);
}
