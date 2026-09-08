#pragma once

#include "Core/CoreTypes.h"
#include "Object/ObjectPtr.h"

class UWorld;

enum class EWorldType : uint8
{
	Editor,
	Game,
	PIE
};

// Engine에 등록된 Context는 생성부터 파괴까지 World 하나를 갖는다.
struct FWorldContext
{
	uint64 ContextId = 0;
	EWorldType WorldType = EWorldType::Editor;
	TObjectPtr<UWorld> World = nullptr;
};
