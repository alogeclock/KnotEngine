#pragma once

#include "EngineAPI.h"
#include "Core/Archive/StructuredArchive.h"
#include "Core/CoreTypes.h"

#include <filesystem>
#include <optional>

class UClass;
class UComponent;
class ULevel;
class UNode;
class UObject;
class UWorld;

// World의 Level, Node와 Component 객체 그래프를 Binary Structured Archive 기반의 .kmap으로 저장하고 복원한다.
// TODO: 추후 UObject를 파일 단위로 묶는 범용 직렬화 구조가 필요할 경우 UPackage 구현을 고려한다.
class ENGINE_API FMapSerializer final
{
public:
	bool Save(UWorld& World, const std::filesystem::path& FilePath) const;
	bool Load(UWorld& World, const std::filesystem::path& FilePath) const;

private:
	// Map에 저장된 UUID와 현재 UObject 포인터를 양방향으로 연결한다.
	class FMapObjectResolver final : public FStructuredArchiveObjectResolver
	{
	public:
		bool Register(uint32 UUID, UObject& Object);
		bool GetObjectUUID(const UObject& Object, uint32& OutUUID) const override;
		UObject* ResolveObject(uint32 UUID, const UClass& ExpectedClass) const override;

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

	// Node 생성과 계층 복원에 필요한 Archive 파싱 결과와 Component 정의를 보관한다.
	struct FMapNodeDefinition
	{
		uint32 UUID = 0;
		uint32 ParentUUID = 0;
		uint32 SiblingIndex = 0;
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

	struct FMapNodeLocation
	{
		FMapNodeDefinition* Node = nullptr;
		SIZE_T LevelIndex = 0;
	};

	static void SerializeObjects(FStructuredArchiveRecord Record, UObject& Object);
	static bool RegisterObjects(UWorld& World, FMapObjectResolver& Resolver);
	static bool ReadMap(FStructuredArchiveRecord Root, FStructuredArchive& Archive, TArray<FMapLevelDefinition>& OutLevels);
	static bool ValidateMap(TArray<FMapLevelDefinition>& Levels);
};
