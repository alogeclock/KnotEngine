#pragma once

#include "EngineAPI.h"

#include "Core/Archive/StructuredArchive.h"

#include <filesystem>
#include <limits>
#include <yaml-cpp/yaml.h>

// YAML 문서를 구조화된 Archive Formatter로 읽고 쓰는 구현이다.
class ENGINE_API FYamlArchiveFormatter final : public FStructuredArchiveFormatter
{
public:
	explicit FYamlArchiveFormatter(EStructuredArchiveMode InMode);
	~FYamlArchiveFormatter() override = default;

	FYamlArchiveFormatter(const FYamlArchiveFormatter&) = delete;
	FYamlArchiveFormatter& operator=(const FYamlArchiveFormatter&) = delete;

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
	YAML::Node* FindNode(FStructuredArchiveElement Element);
	const YAML::Node* FindNode(FStructuredArchiveElement Element) const;
	FStructuredArchiveElement AddNode(const YAML::Node& Node);

	template <typename T>
	bool SerializeScalar(FStructuredArchiveElement Element, T& Value)
	{
		YAML::Node* Node = FindNode(Element);
		if (!Node)
		{
			return false;
		}
		try
		{
			if (Mode == EStructuredArchiveMode::Saving)
			{
				*Node = Value;
			}
			else
			{
				if (!Node->IsScalar())
				{
					return false;
				}
				Value = Node->as<T>();
			}
			return true;
		}
		catch (const YAML::Exception&)
		{
			return false;
		}
	}

	EStructuredArchiveMode Mode;
	YAML::Node Root;
	TArray<YAML::Node> Nodes;
};
