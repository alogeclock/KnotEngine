#pragma once

#include "EngineAPI.h"

#include "Object/Property.h"

class ENGINE_API FNameProperty final : public FProperty
{
public:
	FNameProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InArrayDimension = 1, EPropertyFlags InFlags = EPropertyFlags::None);

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override;
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;
};
