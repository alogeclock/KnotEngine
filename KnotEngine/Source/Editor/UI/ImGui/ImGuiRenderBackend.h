#pragma once

#include "Render/RHI/RenderTypes.h"

struct ImDrawData;

class IImGuiRenderBackend
{
public:
	IImGuiRenderBackend() = default;
	virtual ~IImGuiRenderBackend() = default;

	IImGuiRenderBackend(const IImGuiRenderBackend&) = delete;
	IImGuiRenderBackend& operator=(const IImGuiRenderBackend&) = delete;
	IImGuiRenderBackend(IImGuiRenderBackend&&) = delete;
	IImGuiRenderBackend& operator=(IImGuiRenderBackend&&) = delete;

	// 성공하면 완전히 초기화된 상태로 반환하며, 필수 초기화 실패는 구현부에서 종료한다.
	virtual void Startup() = 0;
	virtual void BeginFrame() = 0;
	virtual void Render(FCommandListHandle CommandList, ImDrawData* DrawData) = 0;
	virtual void Shutdown() = 0;
};
