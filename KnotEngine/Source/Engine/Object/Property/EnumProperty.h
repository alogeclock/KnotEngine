#pragma once

#include "Object/Property.h"

class UEnum;

class FEnumProperty final : public FProperty
{
public:
	FEnumProperty(FName InName, const UStruct* InOwner, uint32 InOffset, const UEnum* InEnum, uint32 InArrayDimension = 1,
		EPropertyFlags InFlags = EPropertyFlags::None);

	const UEnum* GetEnum() const { return Enum; }

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override {}
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;

private:
	const UEnum* Enum = nullptr;
};
