#pragma once

struct FEditorSelection;
class FProperty;
class UObject;

class FInspectorPanel
{
public:
	void Draw(const FEditorSelection& Selection);

private:
	void DrawObject(UObject& Object);
	bool DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject = true);
};
