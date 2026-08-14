#pragma once

#include "Core/CoreTypes.h"

// 전역 FNamePool 안의 문자열 entry 위치를 나타내는 id.
struct FNameEntryId
{
	uint32 Value = 0;

	bool IsNone() const { return Value == 0; }
	uint32 GetBlock() const;
	uint32 GetOffset() const;

	static FNameEntryId Pack(uint32 Block, uint32 Offset);

	bool operator==(const FNameEntryId& Other) const { return Value == Other.Value; }
	bool operator!=(const FNameEntryId& Other) const { return Value != Other.Value; }
};

inline uint32 GetTypeHash(const FNameEntryId& Id) { return Id.Value; }

// 엔진에서 자주 참조되는 Name들을 enum으로 정의하여 lower-case 변환, mutex, 해시맵 조회를 회피.
enum class EName : uint32
{
	None = 0,

	Class,
	ScriptStruct,
	Function,
	Enum,
	Field,
	Property,

	IntProperty,
	FloatProperty,
	BoolProperty,
	EnumProperty,
	ObjectProperty,
    SoftObjectProperty,
	StructProperty,
	ArrayProperty,
	NameProperty,
	StringProperty,
	
	DisplayName,
	Category,
	Tooltip,

	Count
};

// 전역 FNamePool의 entry id와 number suffix만 저장하는 경량 name handle.
class FName
{
public:
	FName() = default;

	static void Startup();
	static void Shutdown();

	explicit FName(const char* InName);
	explicit FName(const FString& InName);
	explicit FName(EName InName);

	bool IsNone() const { return ComparisonIndex == 0 && Number == 0; }
	FString ToString() const;

	bool operator==(const FName& Other) const { return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number; }
	bool operator!=(const FName& Other) const { return !(*this == Other); }

	friend uint32 GetTypeHash(const FName& Name) { return GetTypeHash(FNameEntryId{ Name.ComparisonIndex }) * 33 + Name.Number; }

private:
	struct FNameEntry;
	class FNameEntryAllocator;
	class FNamePool;

	static constexpr uint32 NoneIndex = 0;
	static constexpr uint32 NoNumberInternal = 0;
	static constexpr uint32 BlockOffsetBits = 16;
	static constexpr uint32 BlockSize = (1u << BlockOffsetBits);
	static constexpr uint32 BlockOffsetMask = (BlockSize - 1);
	static constexpr uint32 MaxBlocks = (1u << (32 - BlockOffsetBits));
	static constexpr uint32 MaxLength = 1024;
	static constexpr uint32 NameTableCount = static_cast<uint32>(EName::Count);

	static FNamePool* NamePool;
	static const char* const NameEntries[NameTableCount];
	static FNameEntryId NameEntryMap[NameTableCount];

	static char ToLowerAscii(char Character);
	static bool IsNoneString(const FString& Name);
	static FString MakeComparisonKey(const FString& Name);
	static uint32 ParseNumberFromName(const FString& Name, size_t& InOutPlainNameLength);

	uint32 ComparisonIndex = 0;
	uint32 Number = 0;

	friend struct FNameEntryId;
};
