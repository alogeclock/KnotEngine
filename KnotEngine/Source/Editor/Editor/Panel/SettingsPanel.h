#pragma once

#include "Core/CoreTypes.h"

class FEditorSettings;

// 프로젝트 설정을 Category 목록과 설정 상세 영역으로 표시하는 Modal Panel이다.
class FSettingsPanel final
{
public:
	FSettingsPanel();

	void Open() { bOpenRequested = true; }
	void Draw();

private:
	void DrawGraphics();

	FEditorSettings& Settings;
	TStaticArray<char, 128> SearchText = {};

	bool bOpenRequested = false;
	bool bSaveFailed = false;
	bool bLODStepsInvalid = false;
};
