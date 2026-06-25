#pragma once

#include "Object/Object.h"

class FProperty
{
public:
	virtual ~FProperty() = default;

	virtual void Serialize(FArchive& Ar, void* Container) const = 0;
	virtual void CopyValue(void* Dst, const void* Src, FDuplicateContext& Context) const = 0;
	virtual void VisitReferences(void* Container, FReferenceCollector& Collector) const {}
};

class FObjectProperty : public FProperty
{
private:
	UClass* PropertyClass = nullptr;

public:
	void VisitReferences(void* Container, FReferenceCollector& Collector) const override;
};