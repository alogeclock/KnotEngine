#include "World/MapSerializer.h"

#include "Component/Component.h"
#include "Component/TransformComponent.h"
#include "Core/Archive/BinaryArchiveFormatter.h"
#include "Core/Archive/StructuredArchive.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <charconv>
#include <optional>

// Map에 저장된 UUID와 현재 UObject 포인터를 양방향으로 연결해 객체 참조를 복원한다.
class FMapObjectResolver final : public FStructuredArchiveObjectResolver
{
public:
	bool Register(uint32 UUID, UObject& Object)
	{
		if (UUID == 0 || ObjectsByUUID.contains(UUID) || UUIDsByObject.contains(&Object))
		{
			return false;
		}
		ObjectsByUUID.emplace(UUID, &Object);
		UUIDsByObject.emplace(&Object, UUID);
		return true;
	}

	bool GetObjectUUID(const UObject& Object, uint32& OutUUID) const override
	{
		const auto Iterator = UUIDsByObject.find(&Object);
		if (Iterator == UUIDsByObject.end())
		{
			return false;
		}
		OutUUID = Iterator->second;
		return true;
	}

	UObject* ResolveObject(uint32 UUID, const UClass& ExpectedClass) const override
	{
		const auto Iterator = ObjectsByUUID.find(UUID);
		if (Iterator == ObjectsByUUID.end() || !Iterator->second->IsA(&ExpectedClass))
		{
			return nullptr;
		}
		return Iterator->second;
	}

private:
	TMap<uint32, UObject*> ObjectsByUUID;
	TMap<const UObject*, uint32> UUIDsByObject;
};

// Component를 생성하기 전에 Archive에서 읽은 클래스, 프로퍼티와 저장 UUID를 보관한다.
struct FMapComponentDefinition
{
	uint32 UUID = 0;
	const UClass* Class = nullptr;
	std::optional<FStructuredArchiveRecord> Properties;
	UComponent* Object = nullptr;
};

// Node 생성과 부모 연결에 필요한 Archive 파싱 결과와 소속 Component 정의를 보관한다.
struct FMapNodeDefinition
{
	uint32 UUID = 0;
	uint32 ParentUUID = 0;
	FString Name;
	std::optional<FStructuredArchiveRecord> Properties;
	TArray<FMapComponentDefinition> Components;
	UNode* Object = nullptr;
};

// Level 생성 전에 Archive에서 읽은 저장 UUID와 소속 Node 정의를 보관한다.
struct FMapLevelDefinition
{
	uint32 UUID = 0;
	TArray<FMapNodeDefinition> Nodes;
	ULevel* Object = nullptr;
};

// 객체의 모든 비 Transient 프로퍼티를 구조화된 Record에 직렬화한다.
static void SerializeMapObject(FStructuredArchiveRecord Record, UObject& Object)
{
	TArray<const FProperty*> Properties;
	Object.GetClass()->GetAllProperties(Properties);
	for (const FProperty* Property : Properties)
	{
		if (Property->HasAnyPropertyFlags(EPropertyFlags::Transient))
		{
			continue;
		}
		if (Record.IsSaving())
		{
			Property->SerializeValue(Record.EnterField(Property->GetName()), Property->ContainerPtrToValuePtr(&Object));
		}
		else if (std::optional<FStructuredArchiveSlot> Slot = Record.TryEnterField(Property->GetName()))
		{
			Property->SerializeValue(*Slot, Property->ContainerPtrToValuePtr(&Object));
			Object.PostEditProperty(*Property);
		}
	}
}

// World의 현재 객체 그래프를 각 UObject가 이미 가진 UUID로 등록한다.
static bool RegisterMapObjects(UWorld& World, FMapObjectResolver& Resolver)
{
	const TArray<TObjectPtr<ULevel>>& Levels = World.GetLevels();
	for (const TObjectPtr<ULevel>& Level : Levels)
	{
		if (!Resolver.Register(Level->GetUUID(), *Level))
		{
			return false;
		}
		for (const TObjectPtr<UNode>& Node : Level->GetNodes())
		{
			if (!Resolver.Register(Node->GetUUID(), *Node))
			{
				return false;
			}
			const TArray<TObjectPtr<UComponent>>& Components = Node->GetComponents();
			for (const TObjectPtr<UComponent>& Component : Components)
			{
				if (!Resolver.Register(Component->GetUUID(), *Component))
				{
					return false;
				}
			}
		}
	}
	return true;
}

// Binary Record에서 Map 전체 구조와 생성할 클래스 정보를 읽되 World는 아직 변경하지 않는다.
static bool ReadMapDefinitions(FStructuredArchiveRecord Root, FStructuredArchive& Archive, TArray<FMapLevelDefinition>& OutLevels)
{
	FString Format;
	uint32 Version = 0;
	Root.EnterField("Format") << Format;
	Root.EnterField("Version") << Version;
	if (Archive.HasError() || Format != "KnotMap" || Version != 1)
	{
		return false;
	}

	uint32 LevelCount = 0;
	FStructuredArchiveArray Levels = Root.EnterField("Levels").EnterArray(LevelCount);
	if (Archive.HasError() || LevelCount == 0)
	{
		return false;
	}

	TSet<uint32> ObjectUUIDs;
	TSet<FString> NodeNames;
	OutLevels.resize(LevelCount);
	for (FMapLevelDefinition& LevelDefinition : OutLevels)
	{
		FStructuredArchiveRecord LevelRecord = Levels.EnterElement().EnterRecord();
		LevelRecord.EnterField("UUID") << LevelDefinition.UUID;
		if (LevelDefinition.UUID == 0 || !ObjectUUIDs.emplace(LevelDefinition.UUID).second)
		{
			return false;
		}

		uint32 NodeCount = 0;
		FStructuredArchiveArray Nodes = LevelRecord.EnterField("Nodes").EnterArray(NodeCount);
		LevelDefinition.Nodes.resize(NodeCount);
		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			FStructuredArchiveRecord NodeRecord = Nodes.EnterElement().EnterRecord();
			FString ClassName;
			NodeRecord.EnterField("UUID") << NodeDefinition.UUID;
			NodeRecord.EnterField("Class") << ClassName;
			NodeDefinition.Properties = NodeRecord.EnterField("Properties").EnterRecord();
			std::optional<FStructuredArchiveSlot> NameField = NodeDefinition.Properties->TryEnterField("Name");
			if (NameField)
			{
				*NameField << NodeDefinition.Name;
			}
			if (!NameField || NodeDefinition.UUID == 0 || NodeDefinition.Name.empty() || ClassName != UNode::StaticClass()->GetName() ||
			    !ObjectUUIDs.emplace(NodeDefinition.UUID).second || !NodeNames.emplace(NodeDefinition.Name).second)
			{
				return false;
			}

			if (std::optional<FStructuredArchiveSlot> Parent = NodeRecord.TryEnterField("ParentUUID"); Parent && !Parent->IsNull())
			{
				*Parent << NodeDefinition.ParentUUID;
				if (NodeDefinition.ParentUUID == 0)
				{
					return false;
				}
			}
			uint32 ComponentCount = 0;
			FStructuredArchiveArray Components = NodeRecord.EnterField("Components").EnterArray(ComponentCount);
			NodeDefinition.Components.resize(ComponentCount);
			uint32 TransformCount = 0;
			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				FStructuredArchiveRecord ComponentRecord = Components.EnterElement().EnterRecord();
				ComponentRecord.EnterField("UUID") << ComponentDefinition.UUID;
				ComponentRecord.EnterField("Class") << ClassName;
				ComponentDefinition.Class = GReflectionRegistry ? GReflectionRegistry->FindClass(FName(ClassName)) : nullptr;
				if (ComponentDefinition.UUID == 0 || !ObjectUUIDs.emplace(ComponentDefinition.UUID).second || !ComponentDefinition.Class ||
				    !ComponentDefinition.Class->IsChildOf(UComponent::StaticClass()) || !ComponentDefinition.Class->CanCreateObject())
				{
					return false;
				}
				TransformCount += ComponentDefinition.Class == UTransformComponent::StaticClass() ? 1u : 0u;
				ComponentDefinition.Properties = ComponentRecord.EnterField("Properties").EnterRecord();
			}
			if (TransformCount != 1)
			{
				return false;
			}
		}
	}
	return !Archive.HasError();
}

// World를 Binary Structured Archive 기반의 .kmap 파일로 저장한다.
bool FMapSerializer::Save(UWorld& World, const std::filesystem::path& FilePath) const
{
	if (FilePath.extension() != L".kmap")
	{
		KE_LOG(LogMapSerializer, Error, "Map 파일 확장자가 .kmap이 아니다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	FMapObjectResolver Resolver;
	if (!RegisterMapObjects(World, Resolver))
	{
		KE_LOG(LogMapSerializer, Error, "Map 객체에 유효하지 않거나 중복된 UUID가 있다.");
		return false;
	}

	FBinaryArchiveFormatter Formatter(EStructuredArchiveMode::Saving);
	FStructuredArchive Archive(Formatter, &Resolver);
	FStructuredArchiveRecord Root = Archive.Open().EnterRecord();
	FString Format = "KnotMap";
	uint32 Version = 1;
	Root.EnterField("Format") << Format;
	Root.EnterField("Version") << Version;

	const TArray<TObjectPtr<ULevel>>& WorldLevels = World.GetLevels();
	uint32 LevelCount = static_cast<uint32>(WorldLevels.size());
	FStructuredArchiveArray Levels = Root.EnterField("Levels").EnterArray(LevelCount);
	for (const TObjectPtr<ULevel>& Level : WorldLevels)
	{
		FStructuredArchiveRecord LevelRecord = Levels.EnterElement().EnterRecord();
		uint32 LevelUUID = 0;
		Resolver.GetObjectUUID(*Level, LevelUUID);
		LevelRecord.EnterField("UUID") << LevelUUID;

		const TArray<TObjectPtr<UNode>>& LevelNodes = Level->GetNodes();
		uint32 NodeCount = static_cast<uint32>(LevelNodes.size());
		FStructuredArchiveArray Nodes = LevelRecord.EnterField("Nodes").EnterArray(NodeCount);
		for (const TObjectPtr<UNode>& Node : LevelNodes)
		{
			FStructuredArchiveRecord NodeRecord = Nodes.EnterElement().EnterRecord();
			uint32 NodeUUID = 0;
			FString ClassName = Node->GetClass()->GetName();
			Resolver.GetObjectUUID(*Node, NodeUUID);
			NodeRecord.EnterField("UUID") << NodeUUID;
			NodeRecord.EnterField("Class") << ClassName;

			FStructuredArchiveSlot ParentSlot = NodeRecord.EnterField("ParentUUID");
			if (UTransformComponent* Parent = Node->GetTransform().GetParent())
			{
				uint32 ParentUUID = 0;
				if (!Resolver.GetObjectUUID(Parent->GetOwner(), ParentUUID))
				{
					Archive.SetError();
				}
				ParentSlot << ParentUUID;
			}
			else
			{
				ParentSlot.SetNull();
			}

			SerializeMapObject(NodeRecord.EnterField("Properties").EnterRecord(), *Node);
			const TArray<TObjectPtr<UComponent>>& NodeComponents = Node->GetComponents();
			uint32 ComponentCount = static_cast<uint32>(NodeComponents.size());
			FStructuredArchiveArray Components = NodeRecord.EnterField("Components").EnterArray(ComponentCount);
			for (const TObjectPtr<UComponent>& Component : NodeComponents)
			{
				FStructuredArchiveRecord ComponentRecord = Components.EnterElement().EnterRecord();
				uint32 ComponentUUID = 0;
				ClassName = Component->GetClass()->GetName();
				Resolver.GetObjectUUID(*Component, ComponentUUID);
				ComponentRecord.EnterField("UUID") << ComponentUUID;
				ComponentRecord.EnterField("Class") << ClassName;
				SerializeMapObject(ComponentRecord.EnterField("Properties").EnterRecord(), *Component);
			}
		}
	}

	if (Archive.HasError() || !Formatter.SaveToFile(FilePath))
	{
		KE_LOG(LogMapSerializer, Error, "Binary Map 저장에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}
	KE_LOG(LogMapSerializer, Log, "Map 저장 완료. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
	return true;
}

// Binary .kmap을 검증한 뒤 기존 World의 Level과 Node를 새 객체 그래프로 교체한다.
bool FMapSerializer::Load(UWorld& World, const std::filesystem::path& FilePath) const
{
	if (FilePath.extension() != L".kmap" || World.GetPlayState() != EPlayState::Stopped)
	{
		KE_LOG(LogMapSerializer, Error, "정지 상태의 World에 .kmap 파일만 불러올 수 있다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	FBinaryArchiveFormatter Formatter(EStructuredArchiveMode::Loading);
	if (!Formatter.LoadFromFile(FilePath))
	{
		KE_LOG(LogMapSerializer, Error, "Binary Map을 읽지 못했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	FMapObjectResolver Resolver;
	FStructuredArchive Archive(Formatter, &Resolver);
	TArray<FMapLevelDefinition> LevelDefinitions;
	if (!ReadMapDefinitions(Archive.Open().EnterRecord(), Archive, LevelDefinitions))
	{
		KE_LOG(LogMapSerializer, Error, "Map 구조 또는 클래스 검증에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	World.Reset();

	for (SIZE_T LevelIndex = 0; LevelIndex < LevelDefinitions.size(); ++LevelIndex)
	{
		FMapLevelDefinition& LevelDefinition = LevelDefinitions[LevelIndex];
		LevelDefinition.Object = LevelIndex == 0 ? &World.GetPersistentLevel() : &World.CreateLevel();
		if (!Resolver.Register(LevelDefinition.UUID, *LevelDefinition.Object))
		{
			Archive.SetError();
			break;
		}

		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			NodeDefinition.Object = &LevelDefinition.Object->CreateNode(FName(NodeDefinition.Name));
			if (!Resolver.Register(NodeDefinition.UUID, *NodeDefinition.Object))
			{
				Archive.SetError();
				break;
			}

			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				if (ComponentDefinition.Class == UTransformComponent::StaticClass())
				{
					ComponentDefinition.Object = &NodeDefinition.Object->GetTransform();
				}
				else
				{
					ComponentDefinition.Object = &NodeDefinition.Object->AddComponent(*ComponentDefinition.Class);
				}
				if (!Resolver.Register(ComponentDefinition.UUID, *ComponentDefinition.Object))
				{
					Archive.SetError();
					break;
				}
			}
		}
	}
	if (Archive.HasError())
	{
		KE_LOG(LogMapSerializer, Error, "Map 객체 생성에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	for (FMapLevelDefinition& LevelDefinition : LevelDefinitions)
	{
		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			SerializeMapObject(*NodeDefinition.Properties, *NodeDefinition.Object);
			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				SerializeMapObject(*ComponentDefinition.Properties, *ComponentDefinition.Object);
			}
		}
	}

	for (FMapLevelDefinition& LevelDefinition : LevelDefinitions)
	{
		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			if (NodeDefinition.ParentUUID == 0)
			{
				continue;
			}
			UObject* ParentObject = Resolver.ResolveObject(NodeDefinition.ParentUUID, *UNode::StaticClass());
			if (!ParentObject || !NodeDefinition.Object->GetTransform().SetParent(&static_cast<UNode*>(ParentObject)->GetTransform()))
			{
				Archive.SetError();
			}
		}
	}

	for (const FMapLevelDefinition& LevelDefinition : LevelDefinitions)
	{
		for (const FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			const FString& Name = NodeDefinition.Name;
			SIZE_T SuffixOffset = Name.find_last_of(' ');
			uint64 NextSuffix = 1;
			FString BaseName = Name;
			if (SuffixOffset != FString::npos && SuffixOffset + 1 < Name.size())
			{
				uint64 Suffix = 0;
				const char* First = Name.data() + SuffixOffset + 1;
				const auto Result = std::from_chars(First, Name.data() + Name.size(), Suffix);
				if (Result.ec == std::errc() && Result.ptr == Name.data() + Name.size())
				{
					BaseName = Name.substr(0, SuffixOffset);
					NextSuffix = Suffix + 1;
				}
			}
			World.NameCounters[BaseName] = (std::max)(World.NameCounters[BaseName], NextSuffix);
		}
	}

	if (Archive.HasError())
	{
		KE_LOG(LogMapSerializer, Error, "Map 객체 또는 프로퍼티 복원에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}
	KE_LOG(LogMapSerializer, Log, "Map 불러오기 완료. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
	return true;
}
