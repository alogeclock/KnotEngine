#pragma once

#include "Core/CoreTypes.h"

class UClass;
class UNode;
class UWorld;

// Hierarchy와 Viewport가 공유하는 Node 생성 메뉴를 그린다.
class FNodeCreationMenu final
{
public:
	static UNode* Draw(UWorld& World);
	static UNode* DrawItems(UWorld& World);

private:
	static UNode& CreateNode(UWorld& World, const FString& BaseName);
	static UNode* DrawComponentNode(UWorld& World, const UClass& ComponentClass);
};
