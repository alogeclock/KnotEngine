#pragma once

#include "RendererAPI.h"

#include "Core/CoreTypes.h"

#include <imgui.h>

struct RENDERER_API FImGuiDrawCommand
{
	ImVec4 ClipRect;
	ImTextureID TextureId = {};
	uint32 ElementCount = 0;
	uint32 IndexOffset = 0;
	uint32 VertexOffset = 0;
	bool bResetRenderState = false;
};

struct RENDERER_API FImGuiDrawListCopy
{
	TArray<ImDrawVert> Vertices;
	TArray<ImDrawIdx> Indices;
	TArray<FImGuiDrawCommand> Commands;
};

// Game Thread의 ImGui Context와 무관하게 Render Thread가 소비할 수 있는 Draw Data 복사본이다.
struct RENDERER_API FImGuiDrawDataCopy
{
	void Copy(const ImDrawData* DrawData);
	void Reset();
	bool IsVisible() const;

	ImVec2 DisplayPosition;
	ImVec2 DisplaySize;
	ImVec2 FramebufferScale = ImVec2(1.0f, 1.0f);

	TArray<FImGuiDrawListCopy> DrawLists;
};
