#pragma once

#include "Object/ObjectPtr.h"
#include "Object/Property.h"

class UClass;

class FSoftObjectProperty final : public FProperty
{
public:
	FSoftObjectProperty(
		FName InName,
		const UStruct* InOwner,
		uint32 InOffset,
		const UClass* InPropertyClass,
		const ISoftObjectPtrOps* InSoftObjectPtrOps,
		uint32 InArrayDimension = 1,
		EPropertyFlags InFlags = EPropertyFlags::None);

	const UClass* GetPropertyClass() const { return PropertyClass; }
	const ISoftObjectPtrOps* GetSoftObjectPtrOps() const { return SoftObjectPtrOps; }

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override;
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;

private:
	const UClass* PropertyClass = nullptr;
	const ISoftObjectPtrOps* SoftObjectPtrOps = nullptr;
};
