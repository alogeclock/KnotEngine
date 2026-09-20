#pragma once

#include "EngineAPI.h"

#include "Core/Archive/Archive.h"
#include "Core/Archive/StructuredArchive.h"

#include <filesystem>
#include <span>

// Binary Structured Archive 파일 앞에 저장되는 공통 컨테이너 헤더다.
struct ENGINE_API FBinaryArchiveFileHeader
{
	inline static constexpr char MagicValue[4] = { 'K', 'B', 'A', 'R' };
	inline static constexpr uint32 CurrentVersion = 1;

	char Magic[4] = {};
	uint32 FormatterVersion = 0;
	uint32 NameCount = 0;
	uint32 NameTableSize = 0;
	uint64 PayloadSize = 0;
};
static_assert(sizeof(FBinaryArchiveFileHeader) == 24);

inline FArchive& operator<<(FArchive& Ar, FBinaryArchiveFileHeader& Header)
{
	Ar.Serialize(Header.Magic, sizeof(Header.Magic));
	Ar << Header.FormatterVersion;
	Ar << Header.NameCount;
	Ar << Header.NameTableSize;
	Ar << Header.PayloadSize;
	return Ar;
}

// 필드 이름을 이름 테이블로 중복 제거하고 구조와 값을 Binary Node 그래프로 기록하는 Structured Archive Formatter다.
class ENGINE_API FBinaryArchiveFormatter final : public FStructuredArchiveFormatter
{
public:
	explicit FBinaryArchiveFormatter(EStructuredArchiveMode InMode);
	~FBinaryArchiveFormatter() override = default;

	FBinaryArchiveFormatter(const FBinaryArchiveFormatter&) = delete;
	FBinaryArchiveFormatter& operator=(const FBinaryArchiveFormatter&) = delete;

	bool LoadFromFile(const std::filesystem::path& FilePath);
	bool SaveToFile(const std::filesystem::path& FilePath) const;

	bool IsLoading() const override;
	bool IsSaving() const override;
	FStructuredArchiveElement GetRootElement() const override;

	bool EnterRecord(FStructuredArchiveElement Element) override;
	bool EnterField(FStructuredArchiveElement Record, std::string_view Name, bool bCreate, FStructuredArchiveElement& OutElement) override;
	bool EnterArray(FStructuredArchiveElement Element, uint32& Count) override;
	bool EnterArrayElement(FStructuredArchiveElement Array, uint32 Index, FStructuredArchiveElement& OutElement) override;

	bool IsNull(FStructuredArchiveElement Element) const override;
	bool SetNull(FStructuredArchiveElement Element) override;

	bool Serialize(FStructuredArchiveElement Element, bool& Value) override;
	bool Serialize(FStructuredArchiveElement Element, int32& Value) override;
	bool Serialize(FStructuredArchiveElement Element, uint32& Value) override;
	bool Serialize(FStructuredArchiveElement Element, uint64& Value) override;
	bool Serialize(FStructuredArchiveElement Element, float& Value) override;
	bool Serialize(FStructuredArchiveElement Element, double& Value) override;
	bool Serialize(FStructuredArchiveElement Element, FString& Value) override;

private:
	enum class EValueType : uint8
	{
		Unset,
		Null,
		Record,
		Array,
		Bool,
		Int32,
		UInt32,
		UInt64,
		Float,
		Double,
		String,
	};

	struct FField
	{
		uint16 NameIndex = 0;
		FStructuredArchiveElement Element = 0;
	};

	struct FNode
	{
		EValueType Type = EValueType::Unset;
		TArray<FField> Fields;
		TArray<FStructuredArchiveElement> Elements;
		uint64 Integer = 0;
		double FloatingPoint = 0.0;
		FString String;
	};

	template <typename T>
	bool SerializeScalar(FStructuredArchiveElement Element, EValueType Type, T& Value);
	template <typename T>
	static bool AppendValue(TArray<uint8>& Bytes, const T& Value);
	template <typename T>
	static bool ReadValue(std::span<const uint8> Bytes, SIZE_T& Offset, T& OutValue);

	static bool AppendBytes(TArray<uint8>& Bytes, const void* Data, SIZE_T Size);
	static bool ReadBytes(std::span<const uint8> Bytes, SIZE_T& Offset, void* OutData, SIZE_T Size);

	FNode* FindNode(FStructuredArchiveElement Element);
	const FNode* FindNode(FStructuredArchiveElement Element) const;
	FStructuredArchiveElement AddNode();

	bool WriteNode(FStructuredArchiveElement Element, TArray<uint8>& OutBytes) const;
	bool ReadNode(std::span<const uint8> Bytes, SIZE_T& Offset, FStructuredArchiveElement& OutElement);

	bool FindOrAddName(std::string_view Name, bool bCreate, uint16& OutNameIndex);

	EStructuredArchiveMode Mode;
	TArray<FString> Names;
	TArray<FNode> Nodes;
};
