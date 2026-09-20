#include "Core/Archive/YamlArchiveFormatter.h"

#include <fstream>
#include <system_error>

FYamlArchiveFormatter::FYamlArchiveFormatter(EStructuredArchiveMode InMode) : Mode(InMode)
{
	if (Mode == EStructuredArchiveMode::Saving)
	{
		Root = YAML::Node(YAML::NodeType::Map);
		Nodes.push_back(Root);
	}
}

YAML::Node* FYamlArchiveFormatter::FindNode(FStructuredArchiveElement Element)
{
	return Element < Nodes.size() ? &Nodes[Element] : nullptr;
}

const YAML::Node* FYamlArchiveFormatter::FindNode(FStructuredArchiveElement Element) const
{
	return Element < Nodes.size() ? &Nodes[Element] : nullptr;
}

FStructuredArchiveElement FYamlArchiveFormatter::AddNode(const YAML::Node& Node)
{
	check(Nodes.size() < (std::numeric_limits<uint32>::max)());
	Nodes.push_back(Node);
	return static_cast<FStructuredArchiveElement>(Nodes.size() - 1);
}

// UTF-8 YAML 파일 전체를 DOM으로 파싱하고 Root Element를 준비한다.
bool FYamlArchiveFormatter::LoadFromFile(const std::filesystem::path& FilePath)
{
	if (!IsLoading())
	{
		return false;
	}

	std::ifstream Stream(FilePath, std::ios::binary);
	if (!Stream)
	{
		return false;
	}
	try
	{
		Root = YAML::Load(Stream);
		Nodes.clear();
		Nodes.push_back(Root);
		return Root.IsDefined();
	}
	catch (const YAML::Exception&)
	{
		return false;
	}
}

// 현재 YAML DOM을 두 칸 들여쓰기의 결정적인 텍스트 파일로 기록한다.
bool FYamlArchiveFormatter::SaveToFile(const std::filesystem::path& FilePath) const
{
	if (!IsSaving())
	{
		return false;
	}

	YAML::Emitter Emitter;
	Emitter.SetIndent(2);
	Emitter << Root;
	if (!Emitter.good())
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

	std::ofstream Stream(FilePath, std::ios::binary | std::ios::trunc);
	if (!Stream)
	{
		return false;
	}
	Stream << Emitter.c_str() << '\n';
	return Stream.good();
}

bool FYamlArchiveFormatter::IsLoading() const
{
	return Mode == EStructuredArchiveMode::Loading;
}

bool FYamlArchiveFormatter::IsSaving() const
{
	return Mode == EStructuredArchiveMode::Saving;
}

FStructuredArchiveElement FYamlArchiveFormatter::GetRootElement() const
{
	return 0;
}

bool FYamlArchiveFormatter::EnterRecord(FStructuredArchiveElement Element)
{
	YAML::Node* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		*Node = YAML::Node(YAML::NodeType::Map);
	}
	return Node->IsMap();
}

bool FYamlArchiveFormatter::EnterField(FStructuredArchiveElement Record, std::string_view Name, bool bCreate, FStructuredArchiveElement& OutElement)
{
	YAML::Node* RecordNode = FindNode(Record);
	if (!RecordNode || !RecordNode->IsMap())
	{
		return false;
	}

	const FString FieldName(Name);
	YAML::Node Child = (*RecordNode)[FieldName];
	if (!Child.IsDefined())
	{
		if (!bCreate)
		{
			return false;
		}
		(*RecordNode)[FieldName] = YAML::Node();
		Child = (*RecordNode)[FieldName];
	}
	OutElement = AddNode(Child);
	return true;
}

bool FYamlArchiveFormatter::EnterArray(FStructuredArchiveElement Element, uint32& Count)
{
	YAML::Node* Node = FindNode(Element);
	if (!Node)
	{
		return false;
	}
	if (IsSaving())
	{
		*Node = YAML::Node(YAML::NodeType::Sequence);
		return true;
	}
	if (!Node->IsSequence() || Node->size() > (std::numeric_limits<uint32>::max)())
	{
		return false;
	}
	Count = static_cast<uint32>(Node->size());
	return true;
}

bool FYamlArchiveFormatter::EnterArrayElement(FStructuredArchiveElement Array, uint32 Index, FStructuredArchiveElement& OutElement)
{
	YAML::Node* ArrayNode = FindNode(Array);
	if (!ArrayNode || !ArrayNode->IsSequence())
	{
		return false;
	}
	if (IsSaving())
	{
		if (Index != ArrayNode->size())
		{
			return false;
		}
		ArrayNode->push_back(YAML::Node());
	}
	else if (Index >= ArrayNode->size())
	{
		return false;
	}
	OutElement = AddNode((*ArrayNode)[Index]);
	return true;
}

bool FYamlArchiveFormatter::IsNull(FStructuredArchiveElement Element) const
{
	const YAML::Node* Node = FindNode(Element);
	return Node && Node->IsNull();
}

bool FYamlArchiveFormatter::SetNull(FStructuredArchiveElement Element)
{
	YAML::Node* Node = FindNode(Element);
	if (!Node || !IsSaving())
	{
		return false;
	}
	*Node = YAML::Node(YAML::NodeType::Null);
	return true;
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, bool& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, int32& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, uint32& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, uint64& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, float& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, double& Value)
{
	return SerializeScalar(Element, Value);
}

bool FYamlArchiveFormatter::Serialize(FStructuredArchiveElement Element, FString& Value)
{
	return SerializeScalar(Element, Value);
}
