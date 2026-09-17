#pragma once

#include "Core/CoreTypes.h"

struct FEditorSelection;
struct FQuat;
struct FRotator;
struct FTransform;
struct FVector;
struct ImFont;
class UComponent;
class FProperty;
class UObject;
class UNode;
class UClass;
class FAssetRegistry;

class FInspectorPanel
{
public:
	explicit FInspectorPanel(FAssetRegistry& InAssetRegistry) : AssetRegistry(InAssetRegistry) {}
	~FInspectorPanel();

	void SetBoldFont(ImFont& InBoldFont) { BoldFont = &InBoldFont; }
	void Draw(const FEditorSelection& Selection);

private:
	static bool DrawComponent(UNode& Node, const UClass& Class);

	bool DrawComponentHeader(UComponent& Component, const FString& HeaderName, bool& bRemoveComponent);
	void DrawObject(UObject& Object);
	void DrawAddComponent(UNode& Node);

	void CopyComponent(const UComponent& Component);
	void PasteComponent(UComponent& Component) const;

	static bool ContainsText(const FString& Text, const FString& FilterText);
	static bool DrawFilteredAddComponents(UNode& Node, const TArray<const UClass*>& ComponentClasses, const FString& FilterText, bool& bHasMatch);
	static bool DrawAddComponentMenus(UNode& Node, const TArray<const UClass*>& ComponentClasses);

	void BeginCategory(const FString& CategoryName) const;
	static void EndCategory();

	static bool BeginPropertyRow(const char* Label);
	static void EndPropertyRow();

	static float GetContentPadding();

	bool DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject = true);
	bool DrawVector(const char* Label, FVector& Vector);
	bool DrawRotator(const char* Label, FRotator& Rotator);
	bool DrawQuat(const char* Label, FQuat& Quat);
	bool DrawTransform(FTransform& Transform);

	FAssetRegistry& AssetRegistry;

	ImFont* BoldFont = nullptr;
	UComponent* CopiedComponent = nullptr;
	TStaticArray<char, 128> ComponentFilter = {};
};
