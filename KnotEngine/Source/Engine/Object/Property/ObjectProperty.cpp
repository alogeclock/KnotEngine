#include "ObjectProperty.h"

#include "Asset/Asset/Asset.h"
#include "Asset/AssetManager.h"
#include "Core/Archive/StructuredArchive.h"
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
    : FProperty(std::move(InName), InOwner, InOffset, InObjectPtrOps ? InObjectPtrOps->GetValueSize() : 0, InArrayDimension, InFlags),
      PropertyClass(InPropertyClass),
      ObjectPtrOps(InObjectPtrOps)
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

// UAsset 강한 참조는 영속 ID로, 그 외 UObject 강한 참조는 현재 실행의 UUID로 저장하고 실제 객체로 복원한다.
void FObjectProperty::SerializeElement(FArchive& Ar, void* Value) const
{
	enum class EObjectReferenceKind : uint8
	{
		Null,
		RuntimeObject,
		Asset,
	};

	EObjectReferenceKind Kind = EObjectReferenceKind::Null;
	uint32 ObjectUUID = 0;
	FAssetId AssetId;
	if (Ar.IsSaving())
	{
		if (UObject* Object = ObjectPtrOps->GetObject(Value))
		{
			if (Object->IsA(UAsset::StaticClass()))
			{
				Kind = EObjectReferenceKind::Asset;
				AssetId = static_cast<UAsset*>(Object)->GetAssetId();
				if (!AssetId.IsValid())
				{
					Ar.SetError();
					return;
				}
			}
			else
			{
				Kind = EObjectReferenceKind::RuntimeObject;
				ObjectUUID = Object->GetUUID();
			}
		}
	}

	Ar << Kind;
	if (Ar.HasError() || Kind > EObjectReferenceKind::Asset)
	{
		Ar.SetError();
		return;
	}
	if (Kind == EObjectReferenceKind::Asset)
	{
		Ar << AssetId;
	}
	else if (Kind == EObjectReferenceKind::RuntimeObject)
	{
		Ar << ObjectUUID;
	}
	if (Ar.IsLoading())
	{
		UObject* Object = nullptr;
		if (Kind == EObjectReferenceKind::Asset)
		{
			if (!AssetId.IsValid() || !GAssetManager || !(Object = GAssetManager->LoadAsset(AssetId)) || !Object->IsA(PropertyClass))
			{
				Ar.SetError();
				ObjectPtrOps->SetObject(Value, nullptr);
				return;
			}
		}
		else if (Kind == EObjectReferenceKind::RuntimeObject && ObjectUUID != 0)
		{
			Object = GUObjectManager.FindByUUID(ObjectUUID);
		}
		ObjectPtrOps->SetObject(Value, Object && Object->IsA(PropertyClass) ? Object : nullptr);
	}
}

// Asset은 영속 Asset ID로, Map 내부 객체는 외부 Resolver가 제공하는 UUID로 구조화해 저장하고 복원한다.
void FObjectProperty::SerializeElement(FStructuredArchiveSlot Slot, void* Value) const
{
	if (Slot.IsSaving())
	{
		UObject* Object = ObjectPtrOps->GetObject(Value);
		if (!Object)
		{
			Slot.SetNull();
			return;
		}

		FStructuredArchiveRecord Record = Slot.EnterRecord();
		FString Kind;
		if (Object->IsA(UAsset::StaticClass()))
		{
			Kind = "Asset";
			const FAssetId& AssetId = static_cast<UAsset*>(Object)->GetAssetId();
			if (!AssetId.IsValid())
			{
				Slot.GetArchive().SetError();
				return;
			}
			FString AssetIdText = AssetId.ToString();
			Record.EnterField("Kind") << Kind;
			Record.EnterField("AssetId") << AssetIdText;
		}
		else
		{
			uint32 UUID = 0;
			FStructuredArchiveObjectResolver* Resolver = Slot.GetArchive().GetObjectResolver();
			if (!Resolver || !Resolver->GetObjectUUID(*Object, UUID))
			{
				Slot.GetArchive().SetError();
				return;
			}
			Kind = "Object";
			Record.EnterField("Kind") << Kind;
			Record.EnterField("UUID") << UUID;
		}
		return;
	}

	if (Slot.IsNull())
	{
		ObjectPtrOps->SetObject(Value, nullptr);
		return;
	}

	FStructuredArchiveRecord Record = Slot.EnterRecord();
	FString Kind;
	Record.EnterField("Kind") << Kind;
	if (Slot.GetArchive().HasError())
	{
		ObjectPtrOps->SetObject(Value, nullptr);
		return;
	}

	UObject* Object = nullptr;
	if (Kind == "Asset")
	{
		FString AssetIdText;
		Record.EnterField("AssetId") << AssetIdText;
		FAssetId AssetId;
		if (!FAssetId::TryParse(AssetIdText, AssetId) || !AssetId.IsValid() || !GAssetManager || !(Object = GAssetManager->LoadAsset(AssetId)))
		{
			Slot.GetArchive().SetError();
		}
	}
	else if (Kind == "Object")
	{
		uint32 UUID = 0;
		Record.EnterField("UUID") << UUID;
		FStructuredArchiveObjectResolver* Resolver = Slot.GetArchive().GetObjectResolver();
		Object = Resolver ? Resolver->ResolveObject(UUID, *PropertyClass) : nullptr;
		if (!Object)
		{
			Slot.GetArchive().SetError();
		}
	}
	else
	{
		Slot.GetArchive().SetError();
	}

	if (Object && !Object->IsA(PropertyClass))
	{
		Slot.GetArchive().SetError();
		Object = nullptr;
	}
	ObjectPtrOps->SetObject(Value, Object);
}

// 포인터 저장 형식에서 UObject를 꺼내 강한 참조 수집기에 전달한다.
void FObjectProperty::VisitElementReferences(void* Value, FReferenceCollector& Collector) const
{
	ObjectPtrOps->VisitReference(Value, Collector);
}
