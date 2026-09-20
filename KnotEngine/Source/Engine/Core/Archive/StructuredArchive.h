#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

#include <optional>
#include <string_view>

class UClass;
class UObject;

enum class EStructuredArchiveMode : uint8
{
	Loading,
	Saving,
};

using FStructuredArchiveElement = uint32;

// 구조화된 Archive에서 UObject 참조를 저장 UUID로 변환하고 다시 해석하는 외부 컨텍스트다.
class ENGINE_API FStructuredArchiveObjectResolver
{
public:
	virtual ~FStructuredArchiveObjectResolver() = default;

	virtual bool GetObjectUUID(const UObject& Object, uint32& OutUUID) const = 0;
	virtual UObject* ResolveObject(uint32 UUID, const UClass& ExpectedClass) const = 0;
};

// Record, Field와 Array 구조를 실제 저장 포맷의 노드로 변환하는 인터페이스다.
class ENGINE_API FStructuredArchiveFormatter
{
public:
	virtual ~FStructuredArchiveFormatter() = default;

	virtual bool IsLoading() const = 0;
	virtual bool IsSaving() const = 0;
	virtual FStructuredArchiveElement GetRootElement() const = 0;

	virtual bool EnterRecord(FStructuredArchiveElement Element) = 0;
	virtual bool EnterField(FStructuredArchiveElement Record, std::string_view Name, bool bCreate, FStructuredArchiveElement& OutElement) = 0;
	virtual bool EnterArray(FStructuredArchiveElement Element, uint32& Count) = 0;
	virtual bool EnterArrayElement(FStructuredArchiveElement Array, uint32 Index, FStructuredArchiveElement& OutElement) = 0;

	virtual bool IsNull(FStructuredArchiveElement Element) const = 0;
	virtual bool SetNull(FStructuredArchiveElement Element) = 0;

	virtual bool Serialize(FStructuredArchiveElement Element, bool& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, int32& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, uint32& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, uint64& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, float& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, double& Value) = 0;
	virtual bool Serialize(FStructuredArchiveElement Element, FString& Value) = 0;
};

class FStructuredArchive;
class FStructuredArchiveRecord;
class FStructuredArchiveArray;

// 구조화된 Archive의 값 하나 또는 하위 컨테이너가 들어가는 위치다.
class ENGINE_API FStructuredArchiveSlot
{
public:
	FStructuredArchiveRecord EnterRecord() const;
	FStructuredArchiveArray EnterArray(uint32& Count) const;

	bool IsLoading() const;
	bool IsSaving() const;
	bool IsNull() const;
	void SetNull() const;
	FStructuredArchive& GetArchive() const;

	FStructuredArchiveSlot& operator<<(bool& Value);
	FStructuredArchiveSlot& operator<<(int32& Value);
	FStructuredArchiveSlot& operator<<(uint32& Value);
	FStructuredArchiveSlot& operator<<(uint64& Value);
	FStructuredArchiveSlot& operator<<(float& Value);
	FStructuredArchiveSlot& operator<<(double& Value);
	FStructuredArchiveSlot& operator<<(FString& Value);

private:
	friend class FStructuredArchive;
	friend class FStructuredArchiveRecord;
	friend class FStructuredArchiveArray;

	FStructuredArchiveSlot(FStructuredArchive& InArchive, FStructuredArchiveElement InElement);
	void SerializeResult(bool bSuccess) const;

	FStructuredArchive* Archive = nullptr;
	FStructuredArchiveElement Element = 0;
};

// 이름이 붙은 Field의 집합을 표현한다.
class ENGINE_API FStructuredArchiveRecord
{
public:
	FStructuredArchiveSlot EnterField(std::string_view Name) const;
	std::optional<FStructuredArchiveSlot> TryEnterField(std::string_view Name) const;
	bool IsLoading() const;
	bool IsSaving() const;

private:
	friend class FStructuredArchiveSlot;

	FStructuredArchiveRecord(FStructuredArchive& InArchive, FStructuredArchiveElement InElement);

	FStructuredArchive* Archive = nullptr;
	FStructuredArchiveElement Element = 0;
};

// 순서가 있는 Slot의 집합을 표현한다.
class ENGINE_API FStructuredArchiveArray
{
public:
	FStructuredArchiveSlot EnterElement();
	uint32 GetCount() const { return Count; }
	bool IsLoading() const;
	bool IsSaving() const;

private:
	friend class FStructuredArchiveSlot;

	FStructuredArchiveArray(FStructuredArchive& InArchive, FStructuredArchiveElement InElement, uint32 InCount);

	FStructuredArchive* Archive = nullptr;
	FStructuredArchiveElement Element = 0;
	uint32 Count = 0;
	uint32 NextIndex = 0;
};

// Formatter 위에서 포맷과 무관한 Record, Field와 Array API를 제공한다.
class ENGINE_API FStructuredArchive final
{
public:	
	using FSlot = FStructuredArchiveSlot;
	using FRecord = FStructuredArchiveRecord;
	using FArray = FStructuredArchiveArray;

	explicit FStructuredArchive(FStructuredArchiveFormatter& InFormatter, FStructuredArchiveObjectResolver* InObjectResolver = nullptr);

	FSlot Open();
	bool IsLoading() const { return Formatter.IsLoading(); }
	bool IsSaving() const { return Formatter.IsSaving(); }
	bool HasError() const { return bHasError; }
	void SetError() { bHasError = true; }
	FStructuredArchiveObjectResolver* GetObjectResolver() const { return ObjectResolver; }

private:
	friend class FStructuredArchiveSlot;
	friend class FStructuredArchiveRecord;
	friend class FStructuredArchiveArray;

	FStructuredArchiveFormatter& Formatter;
	FStructuredArchiveObjectResolver* ObjectResolver = nullptr;
	bool bOpened = false;
	bool bHasError = false;
};
