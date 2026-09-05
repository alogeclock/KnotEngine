#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

// Class, UStruct, UEnum 등 UObject 기반 리플렉션 스키마의 메타데이터를 저장하는 구조체.
class ENGINE_API FReflectionMetadata
{
public:
	FReflectionMetadata() = default;
	FReflectionMetadata(FString InDisplayName, FString InCategory, FString InTooltip)
		: DisplayName(std::move(InDisplayName)), Category(std::move(InCategory)), Tooltip(std::move(InTooltip))
	{
	}

	const FString& GetDisplayName() const { return DisplayName; }
	const FString& GetCategory() const { return Category; }
	const FString& GetTooltip() const { return Tooltip; }

	void SetDisplayName(FString InDisplayName) { DisplayName = std::move(InDisplayName); }
	void SetCategory(FString InCategory) { Category = std::move(InCategory); }
	void SetTooltip(FString InTooltip) { Tooltip = std::move(InTooltip); }

	bool IsEmpty() const { return DisplayName.empty() && Category.empty() && Tooltip.empty(); }

private:
	FString DisplayName;
	FString Category;
	FString Tooltip;
};
