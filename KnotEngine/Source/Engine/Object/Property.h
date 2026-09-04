#pragma once

#include "Core/Archive.h"
#include "Object/ReferenceCollector.h"
#include "Object/Reflection/ReflectionMetadata.h"

class UStruct;

enum class EPropertyFlags : uint64
{
	None = 0,
	Transient = 1ull << 0,
	SaveGame = 1ull << 1,
	Parameter = 1ull << 8,
	OutParameter = 1ull << 9,
	ReturnParameter = 1ull << 10,
	NoEdit = 1ull << 16,
};

constexpr EPropertyFlags operator|(EPropertyFlags Lhs, EPropertyFlags Rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint64>(Lhs) | static_cast<uint64>(Rhs));
}

constexpr EPropertyFlags operator&(EPropertyFlags Lhs, EPropertyFlags Rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint64>(Lhs) & static_cast<uint64>(Rhs));
}

constexpr EPropertyFlags& operator|=(EPropertyFlags& Lhs, EPropertyFlags Rhs)
{
	Lhs = Lhs | Rhs;
	return Lhs;
}

// UObject 기반 리플렉션 스키마의 멤버 변수에 대한 메타데이터와 연산을 제공하는 추상 클래스.
class FProperty
{
public:
	FProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InElementSize, uint32 InArrayDimension, EPropertyFlags InFlags);
	virtual ~FProperty() = default;

	const FName& GetFName() const { return Name; }
	FString GetName() const { return Name.ToString(); }
	const UStruct* GetOwner() const { return Owner; }
	FReflectionMetadata& GetMetadata() { return Metadata; }
	const FReflectionMetadata& GetMetadata() const { return Metadata; }

	void* ContainerPtrToValuePtr(void* Container, uint32 ArrayIndex = 0) const;
	const void* ContainerPtrToValuePtr(const void* Container, uint32 ArrayIndex = 0) const;

	void InitializeValue(void* Value) const;
	void DestroyValue(void* Value) const;
	void CopyValue(void* Dst, const void* Src) const;
	void SerializeValue(FArchive& Ar, void* Value) const;
	void VisitReferences(void* Value, FReferenceCollector& Collector) const;
	void SerializeInContainer(FArchive& Ar, void* Container) const;
	void VisitReferencesInContainer(void* Container, FReferenceCollector& Collector) const;

	uint32 GetOffset() const { return Offset; }
	uint32 GetArrayDimension() const { return ArrayDimension; }
	uint32 GetElementSize() const { return ElementSize; }
	uint64 GetSize() const { return static_cast<uint64>(ElementSize) * ArrayDimension; }
	EPropertyFlags GetPropertyFlags() const { return Flags; }
	bool HasAnyPropertyFlags(EPropertyFlags InFlags) const { return (Flags & InFlags) != EPropertyFlags::None; }

protected:
	virtual void InitializeElement(void* Value) const = 0;
	virtual void DestroyElement(void* Value) const = 0;
	virtual void CopyElement(void* Dst, const void* Src) const = 0;
	virtual void SerializeElement(FArchive& Ar, void* Value) const = 0;
	virtual void VisitElementReferences(void* Value, FReferenceCollector& Collector) const {}

private:
	FName Name;
	const UStruct* Owner = nullptr;
	FReflectionMetadata Metadata;
	uint32 Offset = 0;
	uint32 ElementSize = 0;
	uint32 ArrayDimension = 1;
	EPropertyFlags Flags = EPropertyFlags::None;
};
