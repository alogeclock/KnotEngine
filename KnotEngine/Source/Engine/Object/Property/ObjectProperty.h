#pragma once

#include "EngineAPI.h"

#include "Object/ObjectPtr.h"
#include "Object/Property.h"

class UClass;

class ENGINE_API FObjectProperty final : public FProperty
{
public:
	FObjectProperty(
		FName InName,
		const UStruct* InOwner,
		uint32 InOffset,
		const UClass* InPropertyClass,
		const IObjectPtrOps* InObjectPtrOps,
		uint32 InArrayDimension = 1,
		EPropertyFlags InFlags = EPropertyFlags::None);

	const UClass* GetPropertyClass() const { return PropertyClass; }
	const IObjectPtrOps* GetObjectPtrOps() const { return ObjectPtrOps; }

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override;
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;
	void VisitElementReferences(void* Value, FReferenceCollector& Collector) const override;

private:
	const UClass* PropertyClass = nullptr;
	const IObjectPtrOps* ObjectPtrOps = nullptr;
};
