#pragma once

#include "Core/Name.h"
#include "Object/Object.h"
#include "Object/Reflection/ReflectionMetadata.h"

#include <memory>
#include <new>

class FProperty;
class UFunction;
class FArchive;

enum class EClassFlags : uint32
{
	None = 0,
	Abstract = 1 << 0,
	Transient = 1 << 1,
};

constexpr EClassFlags operator|(EClassFlags Lhs, EClassFlags Rhs)
{
	return static_cast<EClassFlags>(static_cast<uint32>(Lhs) | static_cast<uint32>(Rhs));
}

constexpr EClassFlags operator&(EClassFlags Lhs, EClassFlags Rhs)
{
	return static_cast<EClassFlags>(static_cast<uint32>(Lhs) & static_cast<uint32>(Rhs));
}

// UObject 기반 리플렉션 스키마의 공통 기반.
class UField : public UObject
{
public:
	using ThisClass = UField;
	UField(FName InName, UField* InOwner = nullptr);

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	const FName& GetFName() const { return Name; }
	FString GetName() const { return Name.ToString(); }
	UField* GetOwner() const { return Owner; }
	FReflectionMetadata& GetMetadata() { return Metadata; }
	const FReflectionMetadata& GetMetadata() const { return Metadata; }

protected:
	FName Name;
	UField* Owner = nullptr;
	FReflectionMetadata Metadata;

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;
};

class UStruct : public UField
{
public:
	using ThisClass = UStruct;
	UStruct(FName InName, UField* InOwner, const UStruct* InSuperStruct, SIZE_T InStructureSize, SIZE_T InMinAlignment);
	virtual ~UStruct();

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	const UStruct* GetSuperStruct() const { return SuperStruct; }
	SIZE_T GetStructureSize() const { return StructureSize; }
	SIZE_T GetMinAlignment() const { return MinAlignment; }

	FProperty* AddProperty(std::unique_ptr<FProperty> Property);
	const FProperty* FindProperty(const FName& PropertyName) const;
	void GetDeclaredProperties(TArray<const FProperty*>& OutProperties) const;
	void GetAllProperties(TArray<const FProperty*>& OutProperties) const;
	void GetEditorProperties(TArray<const FProperty*>& OutProperties) const;
	void SerializeProperties(FArchive& Ar, void* Container) const;

protected:
	const UStruct* SuperStruct = nullptr;
	SIZE_T StructureSize = 0;
	SIZE_T MinAlignment = 0;
	TArray<std::unique_ptr<FProperty>> Properties;

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;
};

class UClass : public UStruct
{
public:
	using ThisClass = UClass;
	using FCreateObjectFunc = UObject* (*)(UClass* Class);

	UClass(FName InName, UClass* InSuperClass, SIZE_T InClassSize, SIZE_T InMinAlignment, EClassFlags InClassFlags, FCreateObjectFunc InCreateFunc = nullptr);
	~UClass() override;

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	UClass* GetSuperClass() const { return const_cast<UClass*>(static_cast<const UClass*>(GetSuperStruct())); }
	SIZE_T GetClassSize() const { return GetStructureSize(); }
	EClassFlags GetClassFlags() const { return ClassFlags; }

	bool IsChildOf(const UClass* Other) const;
	bool HasAnyClassFlags(EClassFlags Flags) const { return (ClassFlags & Flags) != EClassFlags::None; }
	UObject* CreateObject() const;

	UFunction* AddFunction(std::unique_ptr<UFunction> Function);
	const UFunction* FindFunction(const FName& FunctionName) const;
	void GetAllFunctions(TArray<const UFunction*>& OutFunctions) const;

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;

	EClassFlags ClassFlags = EClassFlags::None;
	FCreateObjectFunc CreateFunc = nullptr;
	TArray<std::unique_ptr<UFunction>> Functions;
};

struct IStructOps
{
	virtual ~IStructOps() = default;

	virtual void Construct(void* Ptr) const = 0;
	virtual void Destruct(void* Ptr) const = 0;
	virtual void Copy(void* Dst, const void* Src) const = 0;
};

template <typename T>
struct TStructOps final : IStructOps
{
	void Construct(void* Ptr) const override
	{
		new (Ptr) T();
	}

	void Destruct(void* Ptr) const override
	{
		static_cast<T*>(Ptr)->~T();
	}

	void Copy(void* Dst, const void* Src) const override
	{
		*static_cast<T*>(Dst) = *static_cast<const T*>(Src);
	}
};

template <typename T>
inline const IStructOps* GetStructOps()
{
	static TStructOps<T> Ops;
	return &Ops;
}

class UScriptStruct : public UStruct
{
public:
	using ThisClass = UScriptStruct;
	UScriptStruct(
		FName InName,
		SIZE_T InSize,
		SIZE_T InAlignment,
		const IStructOps* InStructOps,
		UField* InOwner = nullptr,
		const UScriptStruct* InSuperStruct = nullptr);

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	const IStructOps* GetStructOps() const { return StructOps; }
	void Construct(void* Ptr) const;
	void Destruct(void* Ptr) const;
	void Copy(void* Dst, const void* Src) const;

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;

	const IStructOps* StructOps = nullptr;
};

struct FEnumValue
{
	FName Name;
	FString DisplayName;
	int64 Value = 0;
};

class UEnum : public UField
{
public:
	using ThisClass = UEnum;
	UEnum(FName InName, uint8 InSize, TArray<FEnumValue> InValues, UField* InOwner = nullptr);

	static UClass* StaticClass()
	{
		panic(StaticClassPrivate);
		return StaticClassPrivate;
	}

	uint8 GetSize() const { return Size; }
	const TArray<FEnumValue>& GetValues() const { return Values; }
	const FEnumValue* FindValueByName(const FName& Name) const;

private:
	friend class FReflectionRegistry;

	static UClass* StaticClassPrivate;

	uint8 Size = 0;
	TArray<FEnumValue> Values;
};
