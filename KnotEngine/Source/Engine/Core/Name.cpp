#include "Name.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <unordered_map>
#include <vector>

namespace
{
	constexpr uint32 NAME_NONE_INDEX = 0;
	constexpr uint32 NAME_NO_NUMBER_INTERNAL = 0;

	constexpr uint32 NAME_BLOCK_OFFSET_BITS = 16;
	constexpr uint32 NAME_BLOCK_SIZE = (1u << NAME_BLOCK_OFFSET_BITS);
	constexpr uint32 NAME_BLOCK_OFFSET_MASK = (NAME_BLOCK_SIZE - 1);

	constexpr uint32 NAME_MAX_BLOCKS = (1u << (32 - NAME_BLOCK_OFFSET_BITS));
	constexpr uint32 NAME_MAX_LENGTH = 1024;

	struct FNameEntry
	{
		uint16 Length = 0;

		const char* GetStringData() const { return reinterpret_cast<const char*>(this + 1); }
		char* GetStringData() { return reinterpret_cast<char*>(this + 1); }
		FString ToString() const { return FString(GetStringData(), Length); }
	};

	// FNamePool 내부에서 문자열 entry들을 블록 단위로 저장하는 allocator.
	// 모든 접근은 FNamePool::Mutex로 직렬화되므로 자체 락은 두지 않는다.
	class FNameEntryAllocator
	{
	public:
		FNameEntryAllocator();
		~FNameEntryAllocator();

		FNameEntryAllocator(const FNameEntryAllocator&) = delete;
		FNameEntryAllocator& operator=(const FNameEntryAllocator&) = delete;

		FNameEntryId Allocate(const FString& InName);
		FString Resolve(FNameEntryId Id) const;

	private:
		void AllocateBlock();

	private:
		std::vector<uint8*> Blocks;
		uint32 CurrentBlock = 0;
		uint32 CurrentOffset = 0;
	};

	// 문자열과 FNameEntryId를 매핑하는 process-wide name pool.
	class FNamePool
	{
	public:
		static void Startup();
		static void Shutdown();
		static FNamePool& Get();

		FNameEntryId FindOrAdd(const FString& Name);
		FString Resolve(FNameEntryId Id) const;

	private:
		FNamePool();

	private:
		FNameEntryAllocator Allocator;
		std::unordered_map<FString, FNameEntryId> NameMap;
		mutable std::mutex Mutex;
	};

	FNamePool* GNamePool = nullptr;

	static constexpr const char* GNameEntries[] =
	{
		"None",
		"Class",
		"ScriptStruct",
		"Function",
		"Enum",
		"Field",
		"Property",
		"IntProperty",
		"FloatProperty",
		"BoolProperty",
		"EnumProperty",
		"ObjectProperty",
		"SoftObjectProperty",
		"StructProperty",
		"ArrayProperty",
		"NameProperty",
		"StringProperty",
		"DisplayName",
		"Category",
		"ToolTip",
	};

	constexpr uint32 NAME_TABLE_COUNT = static_cast<uint32>(EName::Count);
	static_assert(sizeof(GNameEntries) / sizeof(GNameEntries[0]) == static_cast<size_t>(EName::Count));

	FNameEntryId GNameEntryMap[NAME_TABLE_COUNT] = {};

	uint32 AlignValue(uint32 Value, uint32 Alignment)
	{
		return (Value + Alignment - 1) & ~(Alignment - 1);
	}

	char ToLowerAscii(char Character)
	{
		return (Character >= 'A' && Character <= 'Z') ? static_cast<char>(Character + ('a' - 'A')) : Character;
	}

	bool IsNoneString(const FString& Name)
	{
		if (Name.empty())
		{
			return true;
		}

		static constexpr char None[] = "none";
		return Name.size() == 4 &&
			std::equal(Name.begin(), Name.end(), None,
				[](char A, char B) { return ToLowerAscii(A) == B; });
	}

	FString MakeComparisonKey(const FString& Name)
	{
		FString Key = Name;
		std::transform(Key.begin(), Key.end(), Key.begin(),
			[](char Character) { return ToLowerAscii(Character); });
		return Key;
	}

	// Name[0, InOutPlainNameLength) 범위에서 뒤쪽 "_<digits>" 형태의 숫자 suffix를 찾아 분리한다.
	// 분리에 성공하면 InOutPlainNameLength를 base 이름 길이로 줄이고, 내부 표현(외부값 + 1)을 반환한다.
	// 분리 조건을 만족하지 못하면 InOutPlainNameLength는 그대로 두고 NAME_NO_NUMBER_INTERNAL을 반환한다.
	uint32 ParseNumberFromName(const FString& Name, size_t& InOutPlainNameLength)
	{
		const size_t End = InOutPlainNameLength;
		size_t DigitStart = End;
		while (DigitStart > 0 && std::isdigit(static_cast<unsigned char>(Name[DigitStart - 1])))
		{
			--DigitStart;
		}

		const size_t DigitCount = End - DigitStart;
		if (DigitCount == 0 || DigitStart < 2 || Name[DigitStart - 1] != '_')
		{
			return NAME_NO_NUMBER_INTERNAL;
		}

		// 선행 0이 있는 숫자는(예: "Actor_007") 분리하지 않는다. "Actor_0"은 허용한다.
		if (DigitCount > 1 && Name[DigitStart] == '0')
		{
			return NAME_NO_NUMBER_INTERNAL;
		}

		uint32 ParsedValue = 0;
		const auto ConvertResult = std::from_chars(Name.data() + DigitStart, Name.data() + End, ParsedValue);

		// std::from_chars가 오버플로우면 errc::result_out_of_range를 반환한다.
		// UINT32_MAX는 internal(+1) 저장 시 0(=번호 없음 sentinel)이므로 별도로 제외한다.
		if (ConvertResult.ec != std::errc() || ParsedValue == std::numeric_limits<uint32>::max())
		{
			return NAME_NO_NUMBER_INTERNAL;
		}

		InOutPlainNameLength = DigitStart - 1;
		return ParsedValue + 1; // 0은 "번호 없음"이므로, 외부 번호는 내부에서 +1로 저장한다.
	}

// 첫 블록을 할당하고, id 0에 "None" 문자열을 등록해 NAME_NONE_INDEX 규약을 보장한다.
FNameEntryAllocator::FNameEntryAllocator()
{
	AllocateBlock();

	const FNameEntryId NoneId = Allocate("None");
	check(NoneId.Value == NAME_NONE_INDEX);
}

// 할당했던 모든 메모리 블록을 해제한다.
FNameEntryAllocator::~FNameEntryAllocator()
{
	for (uint8* Block : Blocks)
	{
		delete[] Block;
	}
}

// 문자열을 현재 블록(공간이 없으면 새 블록)에 정렬된 FNameEntry로 기록하고, 그 위치를 가리키는 FNameEntryId를 반환한다.
FNameEntryId FNameEntryAllocator::Allocate(const FString& InName)
{
    check(InName.size() < NAME_MAX_LENGTH); // 실패 시 호출 쪽 계약 위반, MAX_LENGTH 이하로 요청하도록 수정한다.
    
    const uint32 Length = static_cast<uint32>(InName.size());
    const uint32 EntrySize = static_cast<uint32>(sizeof(FNameEntry)) + Length + 1;
    const uint32 AlignedSize = AlignValue(EntrySize, static_cast<uint32>(alignof(FNameEntry)));
	check(AlignedSize <= NAME_BLOCK_SIZE);

    // 공간이 없을 경우 새로운 블록을 할당한다.
	if (CurrentOffset + AlignedSize > NAME_BLOCK_SIZE)
	{
		AllocateBlock();
	}

	uint8* EntryMemory = Blocks[CurrentBlock] + CurrentOffset;
	FNameEntry* Entry = new (EntryMemory) FNameEntry(); // EntryMemory 위치에 미리 할당된 블록에 객체 생성
	Entry->Length = static_cast<uint16>(Length);

	std::memcpy(Entry->GetStringData(), InName.data(), Length);
	Entry->GetStringData()[Length] = '\0';

	const FNameEntryId Id = FNameEntryId::Pack(CurrentBlock, CurrentOffset);
	CurrentOffset += AlignedSize;
	return Id;
}

// FNameEntryId로부터 블록과 오프셋을 추출하여 해당 FNameEntry를 찾아 문자열을 반환한다.
FString FNameEntryAllocator::Resolve(FNameEntryId Id) const
{
	const uint32 Block = Id.GetBlock();
	const uint32 Offset = Id.GetOffset();

	check(Block < Blocks.size());
	check(Offset < NAME_BLOCK_SIZE);
	check(Blocks[Block] != nullptr);

	const FNameEntry* Entry = reinterpret_cast<const FNameEntry*>(Blocks[Block] + Offset);
	return Entry->ToString();
}

// 새로운 블록을 할당하고, CurrentBlock과 CurrentOffset을 초기화한다.
void FNameEntryAllocator::AllocateBlock()
{
	check(Blocks.size() < NAME_MAX_BLOCKS);

	Blocks.push_back(new uint8[NAME_BLOCK_SIZE]);
	CurrentBlock = static_cast<uint32>(Blocks.size() - 1);
	CurrentOffset = 0;
}

// "none" 문자열을 NAME_NONE_INDEX에 등록하여 FNamePool의 규약을 보장한다.
FNamePool::FNamePool()
{
	NameMap.emplace("none", FNameEntryId{ NAME_NONE_INDEX });
}

void FNamePool::Startup()
{
	check(GNamePool == nullptr);
	GNamePool = new FNamePool();

	for (int32 Index = 0; Index < std::size(GNameEntries); ++Index)
	{
        GNameEntryMap[Index] = GNamePool->FindOrAdd(GNameEntries[Index]);
	}

	check(GNameEntryMap[static_cast<int32>(EName::None)].Value == NAME_NONE_INDEX);
}

void FNamePool::Shutdown()
{
	check(GNamePool != nullptr);
	delete GNamePool;
	GNamePool = nullptr;
}

FNamePool& FNamePool::Get()
{
	check(GNamePool != nullptr);
	return *GNamePool;
}

FNameEntryId FNamePool::FindOrAdd(const FString& Name)
{
	if (IsNoneString(Name))
	{
		return FNameEntryId{ NAME_NONE_INDEX };
	}

	std::lock_guard<std::mutex> Lock(Mutex);

	const FString Key = MakeComparisonKey(Name);
	const auto Found = NameMap.find(Key);
	if (Found != NameMap.end())
	{
		return Found->second;
	}

	const FNameEntryId Id = Allocator.Allocate(Name);
	NameMap.emplace(Key, Id);
	return Id;
}

FString FNamePool::Resolve(FNameEntryId Id) const
{
	std::lock_guard<std::mutex> Lock(Mutex);
	return Allocator.Resolve(Id);
}
}

uint32 FNameEntryId::GetBlock() const
{
	return Value >> NAME_BLOCK_OFFSET_BITS;
}

uint32 FNameEntryId::GetOffset() const
{
	return Value & NAME_BLOCK_OFFSET_MASK;
}

FNameEntryId FNameEntryId::Pack(uint32 Block, uint32 Offset)
{
	check(Block < NAME_MAX_BLOCKS);
	check(Offset < NAME_BLOCK_SIZE);

	return FNameEntryId{ (Block << NAME_BLOCK_OFFSET_BITS) | Offset };
}

void FName::Startup()
{
	FNamePool::Startup();
}

void FName::Shutdown()
{
	FNamePool::Shutdown();
}

FName::FName(const char* InName)
{
	if (!InName)
	{
		return;
	}

	*this = FName(FString(InName));
}

FName::FName(const FString& InName)
{
	size_t PlainNameLength = InName.size();
	const uint32 ParsedNumber = ParseNumberFromName(InName, PlainNameLength);
	const FString PlainName = InName.substr(0, PlainNameLength);

	if (IsNoneString(PlainName))
	{
		ComparisonIndex = NAME_NONE_INDEX;
		Number = ParsedNumber;
		return;
	}

	const FNameEntryId NameId = FNamePool::Get().FindOrAdd(PlainName);
	ComparisonIndex = NameId.Value;
	Number = ParsedNumber;
}

FName::FName(EName InName)
{
	const size_t Index = static_cast<size_t>(InName);
	check(Index < static_cast<size_t>(EName::Count));
	check(GNamePool != nullptr);

	const FNameEntryId Id = GNameEntryMap[Index];
	check(InName == EName::None || Id.Value != NAME_NONE_INDEX);

	ComparisonIndex = Id.Value;
	Number = NAME_NO_NUMBER_INTERNAL;
}

FString FName::ToString() const
{
	FString Result = FNamePool::Get().Resolve(FNameEntryId{ ComparisonIndex });

	if (Number != NAME_NO_NUMBER_INTERNAL)
	{
		Result += "_";
		// 내부 번호는 0을 피하기 위해 외부 값보다 1 크게 저장한다.
		Result += std::to_string(Number - 1);
	}

	return Result;
}
