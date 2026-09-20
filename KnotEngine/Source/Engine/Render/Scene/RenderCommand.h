#pragma once

#include "EngineAPI.h"
#include "Core/CoreTypes.h"

// Scene Primitive의 어느 렌더 상태를 갱신할지 지정한다. 여러 항목을 한 명령에 함께 표시할 수 있다.
enum class ERenderCommandType : uint8
{
	None = 0,
	Transform = 1 << 0,
	Mesh = 1 << 1,
	Material = 1 << 2,
	Visibility = 1 << 3,
	All = Transform | Mesh | Material | Visibility,
};

constexpr ERenderCommandType operator|(ERenderCommandType Left, ERenderCommandType Right)
{
	return static_cast<ERenderCommandType>(static_cast<uint8>(Left) | static_cast<uint8>(Right));
}

constexpr ERenderCommandType& operator|=(ERenderCommandType& Left, ERenderCommandType Right)
{
	Left = Left | Right;
	return Left;
}

constexpr bool HasRenderCommand(ERenderCommandType Value, ERenderCommandType Command)
{
	return (static_cast<uint8>(Value) & static_cast<uint8>(Command)) != 0;
}
