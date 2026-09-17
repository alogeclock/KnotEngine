#pragma once

#include "RendererAPI.h"

#include <memory>

class IImGuiRenderBackend;
class IRenderContext;
class IRenderDevice;
class IShaderFormat;

// 실행 중 사용할 Graphics API 구현체와 그에 종속된 Renderer 서비스를 한 수명으로 묶는다.
class RENDERER_API IRenderBackend
{
public:
	IRenderBackend() = default;
	virtual ~IRenderBackend() = default;

	IRenderBackend(const IRenderBackend&) = delete;
	IRenderBackend& operator=(const IRenderBackend&) = delete;
	IRenderBackend(IRenderBackend&&) = delete;
	IRenderBackend& operator=(IRenderBackend&&) = delete;

	virtual IRenderDevice& GetRenderDevice() = 0;
	virtual IRenderContext& GetRenderContext() = 0;
	virtual IShaderFormat& GetShaderFormat() = 0;
	virtual IImGuiRenderBackend& GetImGuiRenderBackend() = 0;
};

// 현재 플랫폼의 기본 Render Backend를 생성한다.
RENDERER_API std::unique_ptr<IRenderBackend> CreateRenderBackend();
