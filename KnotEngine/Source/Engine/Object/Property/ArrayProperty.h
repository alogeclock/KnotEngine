#pragma once

#include "Object/Property.h"

#include <memory>
#include <type_traits>

struct FArrayOps
{
	void (*Construct)(void* Array) = nullptr;
	void (*Destruct)(void* Array) = nullptr;
	void (*Copy)(void* Dst, const void* Src) = nullptr;
	SIZE_T (*Num)(const void* Array) = nullptr;
	void (*Resize)(void* Array, SIZE_T Num) = nullptr;
	void* (*GetElement)(void* Array, SIZE_T Index) = nullptr;
	SIZE_T ElementSize = 0;
};

template <typename T>
inline const FArrayOps* GetArrayOps()
{
	static_assert(!std::is_same_v<T, bool>, "TArray<bool> requires a dedicated property implementation.");
	static const FArrayOps Ops = {
		[](void* Array) { std::construct_at(static_cast<TArray<T>*>(Array)); },
		[](void* Array) { std::destroy_at(static_cast<TArray<T>*>(Array)); },
		[](void* Dst, const void* Src) { *static_cast<TArray<T>*>(Dst) = *static_cast<const TArray<T>*>(Src); },
		[](const void* Array) { return static_cast<const TArray<T>*>(Array)->size(); },
		[](void* Array, SIZE_T Num) { static_cast<TArray<T>*>(Array)->resize(Num); },
		[](void* Array, SIZE_T Index) -> void* { return &(*static_cast<TArray<T>*>(Array))[Index]; },
		sizeof(T),
	};
	return &Ops;
}

class FArrayProperty final : public FProperty
{
public:
	FArrayProperty(
		FName InName,
		const UStruct* InOwner,
		uint32 InOffset,
		uint32 InArraySize,
		const FArrayOps* InArrayOps,
		std::unique_ptr<FProperty> InInner,
		EPropertyFlags InFlags = EPropertyFlags::None);

	const FProperty* GetInner() const { return Inner.get(); }

protected:
	void InitializeElement(void* Value) const override;
	void DestroyElement(void* Value) const override;
	void CopyElement(void* Dst, const void* Src) const override;
	void SerializeElement(FArchive& Ar, void* Value) const override;
	void VisitElementReferences(void* Value, FReferenceCollector& Collector) const override;

private:
	const FArrayOps* ArrayOps = nullptr;
	std::unique_ptr<FProperty> Inner;
};
