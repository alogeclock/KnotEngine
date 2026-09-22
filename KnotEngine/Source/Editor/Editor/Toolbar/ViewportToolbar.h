#pragma once

#include "Render/RHI/RenderTypes.h"

#include <functional>
#include <imgui.h>

class FEditorViewportClient;
class FRenderSystem;

// 모든 Editor Viewport가 공유하는 Camera, View, Show 및 View Mode Toolbar를 그린다.
class FViewportToolbar final
{
public:
	explicit FViewportToolbar(FRenderSystem& InRenderSystem);

	void Startup();
	void Draw(FEditorViewportClient& ViewportClient, const std::function<void()>& DrawExtension = {});
	void Release();
	bool DrawCoordinateSpaceButton(bool bLocalSpace);

private:
	FTextureHandle LoadIconAtlas(const wchar_t* FileName, uint32 AtlasWidth, uint32 AtlasHeight);
	void DrawViewModeButtons(FEditorViewportClient& ViewportClient);

	FRenderSystem& RenderSystem;
	FTextureHandle ViewModeIcons;
	ImTextureID ViewModeIconsId = {};
	FTextureHandle CoordinateSpaceIcons;
	ImTextureID CoordinateSpaceIconsId = {};
};
