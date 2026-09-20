#pragma once

#include "Core/CoreTypes.h"

class FSettingsPanel;

// Saved/Config/EditorSettings.ini에 저장되는 프로젝트 단위 Editor 설정이다.
class FEditorSettings final
{
public:
	bool Load();
	bool Save() const;

	const TStaticArray<float, 4>& GetLODSteps() const { return LODSteps; }
	void Reset();

private:
	friend class FSettingsPanel;

	static bool IsValidLODSteps(const TStaticArray<float, 4>& Steps);
	static FString Trim(const FString& Value);

	TStaticArray<float, 4> LODSteps = { 0.15f, 0.08f, 0.05f, 0.02f };
};
