#pragma once

#include "Render/RHI/RenderTypes.h"

class IRenderDevice;

class FViewport
{
public:
	explicit FViewport(IRenderDevice& InRenderDevice);
	~FViewport();

	FViewport(const FViewport&) = delete;
	FViewport& operator=(const FViewport&) = delete;

	void Resize(uint32 Width, uint32 Height);
	void Release();

	FTextureHandle GetSceneColorTarget() const { return SceneColorTarget; }
	FTextureHandle GetDisplayColorTarget() const { return DisplayColorTarget; }
	FTextureHandle GetSelectionDepthTarget() const { return SelectionDepthTarget; }
	FTextureHandle GetDepthTarget() const { return DepthTarget; }
	FRenderViewport GetRenderViewport() const;

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

	bool IsValid() const
	{
		return SceneColorTarget.IsValid() && DisplayColorTarget.IsValid() && SelectionDepthTarget.IsValid() && DepthTarget.IsValid();
	}

private:
	IRenderDevice& RenderDevice;

	FTextureHandle SceneColorTarget; // Gamma Correction 이전 Linear Color를 저장하며, Post Process Pass가 SRV로 읽는 Target이다.
	FTextureHandle DisplayColorTarget; // Post Process 결과를 저장하며, Viewport Panel이 표시하는 최종 Target이다.
	FTextureHandle SelectionDepthTarget; // 선택된 Primitive의 실루엣과 Depth를 함께 저장하며, Outline 생성 및 깊이 판정에 사용한다.
	FTextureHandle DepthTarget;

	uint32 Width = 0;
	uint32 Height = 0;
};
