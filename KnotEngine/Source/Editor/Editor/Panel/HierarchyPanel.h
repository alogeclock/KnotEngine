#pragma once

#include "Core/CoreTypes.h"

struct FEditorSelection;
class UClass;
class ULevel;
class UNode;
class UWorld;

class FHierarchyPanel
{
public:
	void Draw(UWorld& World, FEditorSelection& Selection);

private:
	static UNode& CreateNode(UWorld& World, const FString& BaseName);
	static bool DrawNode(UWorld& World, FEditorSelection& Selection, const UClass& ComponentClass);

	void DrawLevel(UWorld& World, ULevel& Level, SIZE_T LevelIndex, FEditorSelection& Selection);
	bool DrawContextMenu(UWorld& World, FEditorSelection& Selection);
};
