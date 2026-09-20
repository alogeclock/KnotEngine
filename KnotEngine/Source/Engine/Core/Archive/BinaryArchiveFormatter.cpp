#include "Core/Archive/BinaryArchiveFormatter.h"

#include "Core/Archive/MemoryArchive.h"

#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

template <typename T>
bool FBinaryArchiveFormatter::AppendValue(TArray<uint8>& Bytes, const T& Value)
{
	static_assert(std::is_trivially_copyable_v<T>);
	if (sizeof(T) > Bytes.max_size() - Bytes.size())
	{
		return false;
	}
	const SIZE_T Offset = Bytes.size();
	Bytes.resize(Offset + sizeof(T));
	std::memcpy(Bytes.data() + Offset, &Value, sizeof(T));
	return true;
}

bool FBinaryArchiveFormatter::AppendBytes(TArray<uint8>& Bytes, const void* Data, SIZE_T Size)
{
	if (Size > Bytes.max_size() - Bytes.size() || (Size > 0 && !Data))
	{
		return false;
	}
	const SIZE_T Offset = Bytes.size();
	Bytes.resize(Offset + Size);
	if (Size > 0)
	{
		std::memcpy(Bytes.data() + Offset, Data, Size);
	}
	return true;
}

template <typename T>
bool FBinaryArchiveFormatter::ReadValue(std::span<const uint8> Bytes, SIZE_T& Offset, T& OutValue)
{
	static_assert(std::is_trivially_copyable_v<T>);
	if (Offset > Bytes.size() || sizeof(T) > Bytes.size() - Offset)
	{
		return false;
	}
	std::memcpy(&OutValue, Bytes.data() + Offset, sizeof(T));
	Offset += sizeof(T);
	return true;
}

bool FBinaryArchiveFormatter::ReadBytes(std::span<const uint8> Bytes, SIZE_T& Offset, void* OutData, SIZE_T Size)
{
	if (Offset > Bytes.size() || Size > Bytes.size() - Offset || (Size > 0 && !OutData))
	{
		return false;
	}
	if (Size > 0)
	{
		std::memcpy(OutData, Bytes.data() + Offset, Size);
	}
	Offset += Size;
	return true;
}

FBinaryArchiveFormatter::FBinaryArchiveFormatter(EStructuredArchiveMode InMode)
    : Mode(InMode)
{
	if (IsSaving())
	{
		Nodes.emplace_back();
	}
}

FBinaryArchiveFormatter::FNode* FBinaryArchiveFormatter::FindNode(FStructuredArchiveElement Element)
{
	return Element < Nodes.size() ? &Nodes[Element] : nullptr;
}

const FBinaryArchiveFormatter::FNode* FBinaryArchiveFormatter::FindNode(FStructuredArchiveElement Element) const
{
	return Element < Nodes.size() ? &Nodes[Element] : nullptr;
}

FStructuredArchiveElement FBinaryArchiveFormatter::AddNode()
{
	check(Nodes.size() < (std::numeric_limits<FStructuredArchiveElement>::max)());
	Nodes.emplace_back();
	return static_cast<FStructuredArchiveElement>(Nodes.size() - 1);
}

bool FBinaryArchiveFormatter::FindOrAddName(std::string_view Name, bool bCreate, uint16& OutNameIndex)
{
	for (SIZE_T Index = 0; Index < Names.size(); ++Index)
	{
		if (Names[Index] == Name)
		{
			OutNameIndex = static_cast<uint16>(Index);
			return true;
		}
	}
	if (!bCreate || Names.size() >= (std::numeric_limits<uint16>::max)())
	{
		return false;
	}
	Names.emplace_back(Name);
	OutNameIndex = static_cast<uint16>(Names.size() - 1);
	return true;
}

// Binary Archive 헤더와 이름 테이블을 검증하고 Payload를 임의 접근 가능한 Node 그래프로 복원한다.
bool FBinaryArchiveFormatter::LoadFromFile(const std::filesystem::path& FilePath)
{
	if (!IsLoading())
	{
		return false;
	}

	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		return false;
	}
	const std::streamoff FileSize = Stream.tellg();
	if (FileSize < static_cast<std::streamoff>(sizeof(FBinaryArchiveFileHeader)) ||
		static_cast<uint64>(FileSize) > (std::numeric_limits<SIZE_T>::max)())
	{
		return false;
	}
	TArray<uint8> FileBytes(static_cast<SIZE_T>(FileSize));
	Stream.seekg(0, std::ios::beg);
	if (!Stream.read(reinterpret_cast<char*>(FileBytes.data()), FileSize))
	{
		return false;
	}

	FMemoryReader FileArchive(FileBytes);
	FBinaryArchiveFileHeader Header = {};
	FileArchive << Header;
	const SIZE_T RemainingSize = FileBytes.size() - sizeof(FBinaryArchiveFileHeader);
	if (FileArchive.HasError() || std::memcmp(Header.Magic, FBinaryArchiveFileHeader::MagicValue, sizeof(Header.Magic)) != 0 ||
		Header.FormatterVersion != FBinaryArchiveFileHeader::CurrentVersion || Header.NameCount > (std::numeric_limits<uint16>::max)() ||
		Header.NameTableSize > RemainingSize || Header.PayloadSize != RemainingSize - Header.NameTableSize)
	{
		return false;
	}

	TArray<uint8> NameTable(Header.NameTableSize);
	TArray<uint8> Payload(static_cast<SIZE_T>(Header.PayloadSize));
	if (!NameTable.empty())
	{
		FileArchive.Serialize(NameTable.data(), static_cast<int64>(NameTable.size()));
	}
	if (!Payload.empty())
	{
		FileArchive.Serialize(Payload.data(), static_cast<int64>(Payload.size()));
	}
	if (FileArchive.HasError())
	{
		return false;
	}

	Names.clear();
	Names.reserve(Header.NameCount);
	SIZE_T NameOffset = 0;
	for (uint32 Index = 0; Index < Header.NameCount; ++Index)
	{
		uint16 Length = 0;
		if (!ReadValue(std::span<const uint8>(NameTable), NameOffset, Length) || Length > NameTable.size() - NameOffset)
		{
			return false;
		}
		FString& Name = Names.emplace_back(Length, '\0');
		if (!ReadBytes(std::span<const uint8>(NameTable), NameOffset, Name.data(), Length))
		{
			return false;
		}
	}
	if (NameOffset != NameTable.size())
	{
		return false;
	}

	Nodes.clear();
	SIZE_T PayloadOffset = 0;
	FStructuredArchiveElement RootElement = 0;
	return ReadNode(std::span<const uint8>(Payload), PayloadOffset, RootElement) && RootElement == 0 && PayloadOffset == Payload.size();
}

// Node 그래프를 이름 테이블과 재귀적인 Binary Payload로 기록한다.
bool FBinaryArchiveFormatter::SaveToFile(const std::filesystem::path& FilePath) const
{
	if (!IsSaving() || Nodes.empty() || Names.size() > (std::numeric_limits<uint16>::max)())
	{
		return false;
	}

	TArray<uint8> NameTable;
	for (const FString& Name : Names)
	{
		if (Name.size() > (std::numeric_limits<uint16>::max)())
		{
			return false;
		}
		const uint16 Length = static_cast<uint16>(Name.size());
		if (!AppendValue(NameTable, Length) || !AppendBytes(NameTable, Name.data(), Name.size()))
		{
			return false;
		}
	}

	TArray<uint8> Payload;
	if (!WriteNode(0, Payload) || NameTable.size() > (std::numeric_limits<uint32>::max)())
	{
		return false;
	}

	std::error_code FileSystemError;
	if (!FilePath.parent_path().empty())
	{
		std::filesystem::create_directories(FilePath.parent_path(), FileSystemError);
		if (FileSystemError)
		{
			return false;
		}
	}

	FBinaryArchiveFileHeader Header = {};
	std::memcpy(Header.Magic, FBinaryArchiveFileHeader::MagicValue, sizeof(Header.Magic));
	Header.FormatterVersion = FBinaryArchiveFileHeader::CurrentVersion;
	Header.NameCount = static_cast<uint32>(Names.size());
	Header.NameTableSize = static_cast<uint32>(NameTable.size());
	Header.PayloadSize = Payload.size();
	TArray<uint8> FileBytes;
	FMemoryWriter FileArchive(FileBytes);
	FileArchive << Header;
	if (!NameTable.empty())
	{
		FileArchive.Serialize(NameTable.data(), static_cast<int64>(NameTable.size()));
	}
	if (!Payload.empty())
	{
		FileArchive.Serialize(Payload.data(), static_cast<int64>(Payload.size()));
	}
	if (FileArchive.HasError())
	{
		return false;
	}

	std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
	return Stream && Stream.write(reinterpret_cast<const char*>(FileBytes.data()), static_cast<std::streamsize>(FileBytes.size()));
}

bool FBinaryArchiveFormatter::IsLoading() const
{
	return Mode == EStructuredArchiveMode::Loading;
}

bool FBinaryArchiveFormatter::IsSaving() const
{
	return Mode == EStructuredArchiveMode::Saving;
}

FStructuredArchiveElement FBinaryArchiveFormatter::GetRootElement() const
{
	return 0;
}

bool FBinaryArchiveFormatter::EnterRecord(FStructuredArchiveElement Element)
{
	FNode* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		Node->Type = EValueType::Record;
	}
	return Node->Type == EValueType::Record;
}

bool FBinaryArchiveFormatter::EnterField(FStructuredArchiveElement Record, std::string_view Name, bool bCreate, FStructuredArchiveElement& OutElement)
{
	FNode* RecordNode = FindNode(Record);
	uint16 NameIndex = 0;
	if (!RecordNode || RecordNode->Type != EValueType::Record || !FindOrAddName(Name, bCreate, NameIndex))
	{
		return false;
	}
	for (const FField& Field : RecordNode->Fields)
	{
		if (Field.NameIndex == NameIndex)
		{
			OutElement = Field.Element;
			return true;
		}
	}
	if (!bCreate)
	{
		return false;
	}
	OutElement = AddNode();
	RecordNode = FindNode(Record);
	RecordNode->Fields.push_back({ NameIndex, OutElement });
	return true;
}

bool FBinaryArchiveFormatter::EnterArray(FStructuredArchiveElement Element, uint32& Count)
{
	FNode* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		Node->Type = EValueType::Array;
		Node->Elements.reserve(Count);
	}
	else if (Node->Elements.size() > (std::numeric_limits<uint32>::max)())
	{
		return false;
	}
	else
	{
		Count = static_cast<uint32>(Node->Elements.size());
	}
	return Node->Type == EValueType::Array;
}

bool FBinaryArchiveFormatter::EnterArrayElement(FStructuredArchiveElement Array, uint32 Index, FStructuredArchiveElement& OutElement)
{
	FNode* ArrayNode = FindNode(Array);
	if (!ArrayNode || ArrayNode->Type != EValueType::Array)
	{
		return false;
	}
	if (IsSaving())
	{
		if (Index != ArrayNode->Elements.size())
		{
			return false;
		}
		OutElement = AddNode();
		ArrayNode = FindNode(Array);
		ArrayNode->Elements.push_back(OutElement);
		return true;
	}
	if (Index >= ArrayNode->Elements.size())
	{
		return false;
	}
	OutElement = ArrayNode->Elements[Index];
	return true;
}

bool FBinaryArchiveFormatter::IsNull(FStructuredArchiveElement Element) const
{
	const FNode* Node = FindNode(Element);
	return Node && Node->Type == EValueType::Null;
}

bool FBinaryArchiveFormatter::SetNull(FStructuredArchiveElement Element)
{
	FNode* Node = FindNode(Element);
	if (!Node || !IsSaving())
	{
		return false;
	}
	Node->Type = EValueType::Null;
	return true;
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, bool& Value)
{
	uint8 BinaryValue = Value ? 1 : 0;
	if (!SerializeScalar(Element, EValueType::Bool, BinaryValue) || BinaryValue > 1)
	{
		return false;
	}
	Value = BinaryValue != 0;
	return true;
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, int32& Value)
{
	return SerializeScalar(Element, EValueType::Int32, Value);
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, uint32& Value)
{
	return SerializeScalar(Element, EValueType::UInt32, Value);
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, uint64& Value)
{
	return SerializeScalar(Element, EValueType::UInt64, Value);
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, float& Value)
{
	return SerializeScalar(Element, EValueType::Float, Value);
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, double& Value)
{
	return SerializeScalar(Element, EValueType::Double, Value);
}

bool FBinaryArchiveFormatter::Serialize(FStructuredArchiveElement Element, FString& Value)
{
	FNode* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		Node->Type = EValueType::String;
		Node->String = Value;
	}
	else if (Node->Type == EValueType::String)
	{
		Value = Node->String;
	}
	return Node->Type == EValueType::String;
}

template <typename T>
bool FBinaryArchiveFormatter::SerializeScalar(FStructuredArchiveElement Element, EValueType Type, T& Value)
{
	FNode* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		Node->Type = Type;
		if constexpr (std::is_floating_point_v<T>)
		{
			Node->FloatingPoint = static_cast<double>(Value);
		}
		else
		{
			Node->Integer = static_cast<uint64>(Value);
		}
	}
	else if (Node->Type == Type)
	{
		if constexpr (std::is_floating_point_v<T>)
		{
			Value = static_cast<T>(Node->FloatingPoint);
		}
		else
		{
			Value = static_cast<T>(Node->Integer);
		}
	}
	return Node->Type == Type;
}

bool FBinaryArchiveFormatter::WriteNode(FStructuredArchiveElement Element, TArray<uint8>& OutBytes) const
{
	const FNode* Node = FindNode(Element);
	if (!Node || Node->Type == EValueType::Unset || !AppendValue(OutBytes, Node->Type))
	{
		return false;
	}
	switch (Node->Type)
	{
	case EValueType::Null: return true;
	case EValueType::Record:
	{
		if (Node->Fields.size() > (std::numeric_limits<uint32>::max)())
		{
			return false;
		}
		const uint32 Count = static_cast<uint32>(Node->Fields.size());
		if (!AppendValue(OutBytes, Count))
		{
			return false;
		}
		for (const FField& Field : Node->Fields)
		{
			if (!AppendValue(OutBytes, Field.NameIndex) || !WriteNode(Field.Element, OutBytes))
			{
				return false;
			}
		}
		return true;
	}
	case EValueType::Array:
	{
		if (Node->Elements.size() > (std::numeric_limits<uint32>::max)())
		{
			return false;
		}
		const uint32 Count = static_cast<uint32>(Node->Elements.size());
		if (!AppendValue(OutBytes, Count))
		{
			return false;
		}
		for (FStructuredArchiveElement Child : Node->Elements)
		{
			if (!WriteNode(Child, OutBytes))
			{
				return false;
			}
		}
		return true;
	}
	case EValueType::Bool:
	{
		const uint8 Value = static_cast<uint8>(Node->Integer);
		return AppendValue(OutBytes, Value);
	}
	case EValueType::Int32:
	{
		const int32 Value = static_cast<int32>(Node->Integer);
		return AppendValue(OutBytes, Value);
	}
	case EValueType::UInt32:
	{
		const uint32 Value = static_cast<uint32>(Node->Integer);
		return AppendValue(OutBytes, Value);
	}
	case EValueType::UInt64: return AppendValue(OutBytes, Node->Integer);
	case EValueType::Float:
	{
		const float Value = static_cast<float>(Node->FloatingPoint);
		return AppendValue(OutBytes, Value);
	}
	case EValueType::Double: return AppendValue(OutBytes, Node->FloatingPoint);
	case EValueType::String:
	{
		if (Node->String.size() > (std::numeric_limits<uint32>::max)())
		{
			return false;
		}
		const uint32 Length = static_cast<uint32>(Node->String.size());
		return AppendValue(OutBytes, Length) && AppendBytes(OutBytes, Node->String.data(), Length);
	}
	default: return false;
	}
}

bool FBinaryArchiveFormatter::ReadNode(std::span<const uint8> Bytes, SIZE_T& Offset, FStructuredArchiveElement& OutElement)
{
	EValueType Type = EValueType::Unset;
	if (!ReadValue(Bytes, Offset, Type) || Type <= EValueType::Unset || Type > EValueType::String)
	{
		return false;
	}
	OutElement = AddNode();
	FNode* Node = FindNode(OutElement);
	Node->Type = Type;
	switch (Type)
	{
	case EValueType::Null: return true;
	case EValueType::Record:
	{
		uint32 Count = 0;
		if (!ReadValue(Bytes, Offset, Count) || Count > Bytes.size() - Offset)
		{
			return false;
		}
		Node->Fields.reserve(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			uint16 NameIndex = 0;
			FStructuredArchiveElement Child = 0;
			if (!ReadValue(Bytes, Offset, NameIndex) || NameIndex >= Names.size() || !ReadNode(Bytes, Offset, Child))
			{
				return false;
			}
			Node = FindNode(OutElement);
			Node->Fields.push_back({ NameIndex, Child });
		}
		return true;
	}
	case EValueType::Array:
	{
		uint32 Count = 0;
		if (!ReadValue(Bytes, Offset, Count) || Count > Bytes.size() - Offset)
		{
			return false;
		}
		Node->Elements.reserve(Count);
		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FStructuredArchiveElement Child = 0;
			if (!ReadNode(Bytes, Offset, Child))
			{
				return false;
			}
			Node = FindNode(OutElement);
			Node->Elements.push_back(Child);
		}
		return true;
	}
	case EValueType::Bool:
	{
		uint8 Value = 0;
		if (!ReadValue(Bytes, Offset, Value) || Value > 1)
		{
			return false;
		}
		Node->Integer = Value;
		return true;
	}
	case EValueType::Int32:
	{
		int32 Value = 0;
		if (!ReadValue(Bytes, Offset, Value))
		{
			return false;
		}
		Node->Integer = static_cast<uint64>(Value);
		return true;
	}
	case EValueType::UInt32:
	{
		uint32 Value = 0;
		if (!ReadValue(Bytes, Offset, Value))
		{
			return false;
		}
		Node->Integer = Value;
		return true;
	}
	case EValueType::UInt64: return ReadValue(Bytes, Offset, Node->Integer);
	case EValueType::Float:
	{
		float Value = 0.0f;
		if (!ReadValue(Bytes, Offset, Value))
		{
			return false;
		}
		Node->FloatingPoint = Value;
		return true;
	}
	case EValueType::Double: return ReadValue(Bytes, Offset, Node->FloatingPoint);
	case EValueType::String:
	{
		uint32 Length = 0;
		if (!ReadValue(Bytes, Offset, Length) || Length > Bytes.size() - Offset)
		{
			return false;
		}
		Node->String.resize(Length);
		return ReadBytes(Bytes, Offset, Node->String.data(), Length);
	}
	default: return false;
	}
}
