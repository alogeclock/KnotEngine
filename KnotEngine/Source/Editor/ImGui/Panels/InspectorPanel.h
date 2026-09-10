#pragma once

struct FEditorSelection;
struct FQuat;
struct FTransform;
struct FVector;
class FProperty;
class UObject;

class FInspectorPanel
{
public:
	void Draw(const FEditorSelection& Selection);

private:
	void DrawObject(UObject& Object);
	bool DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject = true);
	bool DrawVector(const char* Label, FVector& Vector);
	bool DrawQuat(const char* Label, FQuat& Quat);
	bool DrawTransform(const char* Label, FTransform& Transform);
};
