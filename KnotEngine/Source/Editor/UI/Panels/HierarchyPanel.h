#pragma once

struct FEditorSelection;
class UWorld;

class FHierarchyPanel
{
public:
	void Draw(UWorld& World, FEditorSelection& Selection);
};
