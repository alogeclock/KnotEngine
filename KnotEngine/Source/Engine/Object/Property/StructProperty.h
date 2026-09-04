#pragma once

#include "Object/Property.h"

class UScriptStruct;

class FStructProperty final : public FProperty
{
public:
	FStructProperty(
		FName InName,
		const UStruct* InOwner,
		uint32 InOffset,
		const UScriptStruct* InStruct,
		uint32 InArrayDimension = 1,
		EPropertyFlags InFlags = EPropertyFlags::None);

	const UScriptStruct* GetStruct() const { return Struct; }

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override;
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;
	void VisitElementReferences(void* Value, FReferenceCollector& Collector) const override;

private:
	const UScriptStruct* Struct = nullptr;
};
