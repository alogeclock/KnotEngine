#pragma once

#include "Core/CoreTypes.h"
#include "Core/Geometry/Transform.h"
#include "Core/Name.h"

#include <variant>

class UObject;
class UComponent;
class ULevel;
class UNode;

// UObject의 직렬화된 변경 전후 상태를 보관한다.
struct FObjectTransactionRecord
{
	UObject* Object = nullptr;
	TArray<uint8> BeforeState;
	TArray<uint8> AfterState;
};

// Node를 계층에 다시 연결할 부모와 형제 순서를 보관한다.
struct FNodePlacement
{
	UNode* Parent = nullptr;
	SIZE_T SiblingIndex = 0;
};

// Node의 계층 위치와 상대 Transform을 한 시점의 상태로 보관한다.
struct FNodeHierarchyState
{
	FNodePlacement Placement;
	FTransform RelativeTransform;
};

// Node의 계층 변경 전후 상태를 보관한다.
struct FHierarchyTransactionRecord
{
	UNode* Node = nullptr;
	FNodeHierarchyState BeforeState;
	FNodeHierarchyState AfterState;
};

// Redo 방향에서 객체가 계층에 연결되는지 분리되는지 나타낸다.
enum class EAttachmentChange : uint8
{
	Added,
	Removed,
};

// Node의 Level 연결 변경과 다시 연결할 위치를 보관한다.
struct FNodeAttachmentRecord
{
	ULevel* Level = nullptr;
	UNode* Node = nullptr;
	FNodePlacement Placement;
	EAttachmentChange Change = EAttachmentChange::Added;
};

// Component의 소유 Node 연결 변경과 다시 연결할 순서를 보관한다.
struct FComponentAttachmentRecord
{
	UNode* Owner = nullptr;
	UComponent* Component = nullptr;
	SIZE_T Index = 0;
	EAttachmentChange Change = EAttachmentChange::Added;
};

// 한 편집 작업에서 순서대로 적용할 수 있는 Record 종류를 묶는다.
using FEditorTransactionRecord = std::variant<FObjectTransactionRecord, FHierarchyTransactionRecord, FNodeAttachmentRecord, FComponentAttachmentRecord>;

// 하나의 Undo/Redo 단계에 포함되는 Record와 표시 이름을 보관한다.
struct FEditorTransaction
{
	FName Description;
	TArray<FEditorTransactionRecord> Records;
};
