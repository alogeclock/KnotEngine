#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Vector2.h"
#include <optional>

struct FEditorSelection;
struct FQuat;
struct FRotator;
struct FTransform;
struct FVector;
struct ImFont;
class UComponent;
class UStaticMeshComponent;
class FProperty;
class UObject;
class UNode;
class UClass;
class FAssetRegistry;
class FInputSnapshot;
class FTransactionManager;

class FInspectorPanel
{
public:
	FInspectorPanel(FAssetRegistry& InAssetRegistry, FTransactionManager& InTransactionManager)
		: AssetRegistry(InAssetRegistry), TransactionManager(InTransactionManager) {}
	~FInspectorPanel();

	void SetBoldFont(ImFont& InBoldFont) { BoldFont = &InBoldFont; }
	void Draw(const FEditorSelection& Selection);
	std::optional<FVector2> FinishCursorDrag(const FInputSnapshot& InputSnapshot);

	bool IsCursorDragging() const { return bCursorDragging; }

private:
	bool DragFloat(const char* Label, float* Value, float Speed, float Min = 0.0f, float Max = 0.0f, const char* Format = "%.3f");
	void TrackCursorDrag();
	bool DrawComponent(UNode& Node, const UClass& Class);

	bool DrawComponentHeader(UComponent& Component, const FString& HeaderName, bool& bRemoveComponent);
	void DrawObject(UObject& Object);
	void DrawAddComponent(UNode& Node);

	void CopyComponent(const UComponent& Component);
	void PasteComponent(UComponent& Component) const;

	static bool ContainsText(const FString& Text, const FString& FilterText);
	bool DrawFilteredAddComponents(UNode& Node, const TArray<const UClass*>& ComponentClasses, const FString& FilterText, bool& bHasMatch);
	bool DrawAddComponentMenus(UNode& Node, const TArray<const UClass*>& ComponentClasses);

	void BeginCategory(const FString& CategoryName) const;
	static void EndCategory();

	static bool BeginPropertyRow(const char* Label);
	static void EndPropertyRow();

	static float GetContentPadding();

	bool DrawProperty(UObject& Object, const FProperty& Property, void* Container, bool bNotifyObject = true, const char* LabelOverride = nullptr);
	bool DrawVector(const char* Label, FVector& Vector);
	bool DrawRotator(const char* Label, FRotator& Rotator);
	bool DrawQuat(const char* Label, FQuat& Quat);
	bool DrawTransform(FTransform& Transform);

	bool DrawStaticMeshMaterials(UStaticMeshComponent& Component);

	FAssetRegistry& AssetRegistry;
	FTransactionManager& TransactionManager;
	UObject* TransactionObject = nullptr;

	ImFont* BoldFont = nullptr;
	UComponent* CopiedComponent = nullptr;
	TStaticArray<char, 128> ComponentFilter = {};

	FVector2 CursorDragOrigin = FVector2::ZeroVector;
	uint32 ActiveDragItemId = 0;
	uint32 PreviousDragItemId = 0;
	bool bDragItemActivated = false;
	bool bCursorDragging = false;
};
