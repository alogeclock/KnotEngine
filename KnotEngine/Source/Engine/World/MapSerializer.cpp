#include "World/MapSerializer.h"

#include "Component/Component.h"
#include "Component/TransformComponent.h"
#include "Core/Archive/BinaryArchiveFormatter.h"
#include "Core/Archive/StructuredArchive.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Object/Reflection/Class.h"
#include "Object/Property.h"
#include "Object/Reflection/ReflectionRegistry.h"
#include "World/Level.h"
#include "World/Node.h"
#include "World/World.h"

#include <algorithm>
#include <optional>

// Map UUID와 UObject를 양방향으로 등록한다.
bool FMapSerializer::FMapObjectResolver::Register(uint32 UUID, UObject& Object)
{
	if (UUID == 0 || ObjectsByUUID.contains(UUID) || UUIDsByObject.contains(&Object))
	{
		return false;
	}
	ObjectsByUUID.emplace(UUID, &Object);
	UUIDsByObject.emplace(&Object, UUID);
	return true;
}

// UObject에 대응하는 Map UUID를 반환한다.
bool FMapSerializer::FMapObjectResolver::GetObjectUUID(const UObject& Object, uint32& OutUUID) const
{
	const auto Iterator = UUIDsByObject.find(&Object);
	if (Iterator == UUIDsByObject.end())
	{
		return false;
	}
	OutUUID = Iterator->second;
	return true;
}

// Map UUID와 기대 클래스에 일치하는 UObject를 해석한다.
UObject* FMapSerializer::FMapObjectResolver::ResolveObject(uint32 UUID, const UClass& ExpectedClass) const
{
	const auto Iterator = ObjectsByUUID.find(UUID);
	if (Iterator == ObjectsByUUID.end() || !Iterator->second->IsA(&ExpectedClass))
	{
		return nullptr;
	}
	return Iterator->second;
}

// 객체의 모든 비 Transient 프로퍼티를 구조화된 Record에 직렬화한다.
void FMapSerializer::SerializeObjects(FStructuredArchiveRecord Record, UObject& Object)
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
bool FMapSerializer::RegisterObjects(UWorld& World, FMapObjectResolver& Resolver)
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
bool FMapSerializer::ReadMap(FStructuredArchiveRecord Root, FStructuredArchive& Archive, TArray<FMapLevelDefinition>& OutLevels)
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
			NodeDefinition.Class = GReflectionRegistry ? GReflectionRegistry->FindClass(FName(ClassName)) : nullptr;
			NodeDefinition.Properties = NodeRecord.EnterField("Properties").EnterRecord();
			std::optional<FStructuredArchiveSlot> NameField = NodeDefinition.Properties->TryEnterField("Name");
			if (NameField)
			{
				*NameField << NodeDefinition.Name;
			}
			if (!NameField || NodeDefinition.UUID == 0 || NodeDefinition.Name.empty() || !NodeDefinition.Class ||
			    !NodeDefinition.Class->IsChildOf(UNode::StaticClass()) || !NodeDefinition.Class->CanCreateObject() ||
			    !ObjectUUIDs.emplace(NodeDefinition.UUID).second)
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
			NodeRecord.EnterField("SiblingIndex") << NodeDefinition.SiblingIndex;
			uint32 ComponentCount = 0;
			FStructuredArchiveArray Components = NodeRecord.EnterField("Components").EnterArray(ComponentCount);
			NodeDefinition.Components.resize(ComponentCount);
			uint32 TransformCount = 0;
			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				FStructuredArchiveRecord ComponentRecord = Components.EnterElement().EnterRecord();
				ComponentRecord.EnterField("UUID") << ComponentDefinition.UUID;
				ComponentRecord.EnterField("Name") << ComponentDefinition.Name;
				ComponentRecord.EnterField("Class") << ClassName;
				ComponentRecord.EnterField("DefaultSubobject") << ComponentDefinition.bDefaultSubobject;
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

// World를 변경하기 전에 Parent 참조, Level 경계, 순환과 Sibling Index 연속성을 모두 검증한다.
bool FMapSerializer::ValidateMap(TArray<FMapLevelDefinition>& Levels)
{
	TMap<uint32, FMapNodeLocation> NodesByUUID;
	for (SIZE_T LevelIndex = 0; LevelIndex < Levels.size(); ++LevelIndex)
	{
		for (FMapNodeDefinition& Node : Levels[LevelIndex].Nodes)
		{
			NodesByUUID.emplace(Node.UUID, FMapNodeLocation{ &Node, LevelIndex });
		}
	}

	for (SIZE_T LevelIndex = 0; LevelIndex < Levels.size(); ++LevelIndex)
	{
		TMap<uint32, TSet<uint32>> SiblingIndices;
		for (const FMapNodeDefinition& Node : Levels[LevelIndex].Nodes)
		{
			if (Node.ParentUUID != 0)
			{
				const auto Parent = NodesByUUID.find(Node.ParentUUID);
				if (Parent == NodesByUUID.end() || Parent->second.LevelIndex != LevelIndex || Parent->second.Node == &Node)
				{
					return false;
				}
			}
			if (!SiblingIndices[Node.ParentUUID].emplace(Node.SiblingIndex).second)
			{
				return false;
			}
		}
		for (const auto& [ParentUUID, Indices] : SiblingIndices)
		{
			for (uint32 Index = 0; Index < Indices.size(); ++Index)
			{
				if (!Indices.contains(Index))
				{
					return false;
				}
			}
		}
	}

	TMap<uint32, uint8> VisitStates;
	for (const auto& [UUID, Location] : NodesByUUID)
	{
		TArray<uint32> Path;
		uint32 CurrentUUID = UUID;
		while (CurrentUUID != 0 && VisitStates[CurrentUUID] != 2)
		{
			if (VisitStates[CurrentUUID] == 1)
			{
				return false;
			}
			VisitStates[CurrentUUID] = 1;
			Path.push_back(CurrentUUID);
			CurrentUUID = NodesByUUID.at(CurrentUUID).Node->ParentUUID;
		}
		for (uint32 PathUUID : Path)
		{
			VisitStates[PathUUID] = 2;
		}
	}
	return true;
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
	if (!RegisterObjects(World, Resolver))
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
			if (UTransformComponent* Parent = Node->GetParent())
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
			uint32 SiblingIndex = static_cast<uint32>(Node->GetSiblingIndex());
			NodeRecord.EnterField("SiblingIndex") << SiblingIndex;

			SerializeObjects(NodeRecord.EnterField("Properties").EnterRecord(), *Node);
			const TArray<TObjectPtr<UComponent>>& NodeComponents = Node->GetComponents();
			uint32 ComponentCount = static_cast<uint32>(NodeComponents.size());
			FStructuredArchiveArray Components = NodeRecord.EnterField("Components").EnterArray(ComponentCount);
			for (const TObjectPtr<UComponent>& Component : NodeComponents)
			{
				FStructuredArchiveRecord ComponentRecord = Components.EnterElement().EnterRecord();
				uint32 ComponentUUID = 0;
				ClassName = Component->GetClass()->GetName();
				FString ComponentName = Component->GetObjectName().ToString();
				bool bDefaultSubobject = Component->HasAnyFlags(EObjectFlags::DefaultSubobject);
				Resolver.GetObjectUUID(*Component, ComponentUUID);
				ComponentRecord.EnterField("UUID") << ComponentUUID;
				ComponentRecord.EnterField("Name") << ComponentName;
				ComponentRecord.EnterField("Class") << ClassName;
				ComponentRecord.EnterField("DefaultSubobject") << bDefaultSubobject;
				SerializeObjects(ComponentRecord.EnterField("Properties").EnterRecord(), *Component);
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
	if (!ReadMap(Archive.Open().EnterRecord(), Archive, LevelDefinitions) || !ValidateMap(LevelDefinitions))
	{
		KE_LOG(LogMapSerializer, Error, "Map 구조, 클래스 또는 계층 검증에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	UWorld* LoadedWorld = NewObject<UWorld>();

	for (SIZE_T LevelIndex = 0; LevelIndex < LevelDefinitions.size(); ++LevelIndex)
	{
		FMapLevelDefinition& LevelDefinition = LevelDefinitions[LevelIndex];
		LevelDefinition.Object = LevelIndex == 0 ? &LoadedWorld->GetPersistentLevel() : &LoadedWorld->CreateLevel();
		if (!Resolver.Register(LevelDefinition.UUID, *LevelDefinition.Object))
		{
			Archive.SetError();
			break;
		}

		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			NodeDefinition.Object = &LevelDefinition.Object->CreateNode(*NodeDefinition.Class, FName(NodeDefinition.Name));
			if (!Resolver.Register(NodeDefinition.UUID, *NodeDefinition.Object))
			{
				Archive.SetError();
				break;
			}

			SIZE_T DefaultSubobjectCount = 0;
			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				if (ComponentDefinition.bDefaultSubobject)
				{
					++DefaultSubobjectCount;
					UObject* DefaultSubobject = NodeDefinition.Object->GetDefaultSubobject(FName(ComponentDefinition.Name));
					if (!DefaultSubobject || DefaultSubobject->GetClass() != ComponentDefinition.Class ||
					    !DefaultSubobject->IsA(UComponent::StaticClass()))
					{
						Archive.SetError();
						break;
					}
					ComponentDefinition.Object = static_cast<UComponent*>(DefaultSubobject);
				}
				else
				{
					ComponentDefinition.Object = &NodeDefinition.Object->AddComponent(*ComponentDefinition.Class, FName(ComponentDefinition.Name));
				}
				if (!Resolver.Register(ComponentDefinition.UUID, *ComponentDefinition.Object))
				{
					Archive.SetError();
					break;
				}
			}
			if (DefaultSubobjectCount != NodeDefinition.Object->GetDefaultSubobjects().size())
			{
				Archive.SetError();
				break;
			}
		}
	}
	if (Archive.HasError())
	{
		GUObjectManager.Destroy(LoadedWorld);
		KE_LOG(LogMapSerializer, Error, "Map 객체 생성에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}

	for (FMapLevelDefinition& LevelDefinition : LevelDefinitions)
	{
		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			SerializeObjects(*NodeDefinition.Properties, *NodeDefinition.Object);
			for (FMapComponentDefinition& ComponentDefinition : NodeDefinition.Components)
			{
				SerializeObjects(*ComponentDefinition.Properties, *ComponentDefinition.Object);
			}
		}
	}

	TArray<FMapNodeDefinition*> ChildDefinitions;
	TArray<FMapNodeDefinition*> RootDefinitions;
	for (FMapLevelDefinition& LevelDefinition : LevelDefinitions)
	{
		for (FMapNodeDefinition& NodeDefinition : LevelDefinition.Nodes)
		{
			if (NodeDefinition.ParentUUID == 0)
			{
				RootDefinitions.push_back(&NodeDefinition);
				continue;
			}
			ChildDefinitions.push_back(&NodeDefinition);
		}
	}
	std::sort(ChildDefinitions.begin(), ChildDefinitions.end(), [](const FMapNodeDefinition* Left, const FMapNodeDefinition* Right)
	{
		return Left->ParentUUID != Right->ParentUUID ? Left->ParentUUID < Right->ParentUUID : Left->SiblingIndex < Right->SiblingIndex;
	});
	for (FMapNodeDefinition* NodeDefinition : ChildDefinitions)
	{
		UObject* ParentObject = Resolver.ResolveObject(NodeDefinition->ParentUUID, *UNode::StaticClass());
		if (!ParentObject || !NodeDefinition->Object->GetTransform().SetParentRelative(
			&static_cast<UNode*>(ParentObject)->GetTransform(), NodeDefinition->SiblingIndex))
		{
			Archive.SetError();
		}
	}
	std::sort(RootDefinitions.begin(), RootDefinitions.end(), [](const FMapNodeDefinition* Left, const FMapNodeDefinition* Right)
	{
		if (&Left->Object->GetLevel() != &Right->Object->GetLevel())
		{
			return Left->Object->GetLevel().GetUUID() < Right->Object->GetLevel().GetUUID();
		}
		return Left->SiblingIndex < Right->SiblingIndex;
	});
	for (FMapNodeDefinition* NodeDefinition : RootDefinitions)
	{
		if (!NodeDefinition->Object->GetTransform().SetSiblingIndex(NodeDefinition->SiblingIndex))
		{
			Archive.SetError();
		}
	}

	if (Archive.HasError())
	{
		GUObjectManager.Destroy(LoadedWorld);
		KE_LOG(LogMapSerializer, Error, "Map 객체 또는 프로퍼티 복원에 실패했다. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
		return false;
	}
	World.Replace(*LoadedWorld);
	GUObjectManager.Destroy(LoadedWorld);
	KE_LOG(LogMapSerializer, Log, "Map 불러오기 완료. Path={}", FPaths::ToUtf8(FilePath.generic_wstring()));
	return true;
}
