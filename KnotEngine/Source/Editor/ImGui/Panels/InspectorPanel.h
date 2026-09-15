#pragma once

#include "Core/CoreTypes.h"

struct FEditorSelection;
struct FQuat;
struct FRotator;
struct FTransform;
struct FVector;
class FProperty;
class UObject;
class UNode;
class UClass;

class FInspectorPanel
{
public:
	void Draw(const FEditorSelection& Selection);

private:
	static bool DrawComponent(UNode& Node, const UClass& Class);

	void DrawObject(UObject& Object);
	void DrawAddComponent(UNode& Node);
	bool DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject = true);
	bool DrawVector(const char* Label, FVector& Vector);
	bool DrawRotator(const char* Label, FRotator& Rotator);
	bool DrawQuat(const char* Label, FQuat& Quat);
	bool DrawTransform(const char* Label, const char* Tooltip, FTransform& Transform);

	TStaticArray<char, 128> ComponentFilter = {};
};
