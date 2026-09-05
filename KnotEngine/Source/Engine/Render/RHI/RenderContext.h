#pragma once

#include "EngineAPI.h"

#include "Render/RHI/RenderTypes.h"

// Native Window에 연결되는 Surface, Swap Chain 및 화면 출력 대상을 관리한다.
// 일반 GPU 자원과 명령은 IRenderDevice에 두어 창 수명과 Device 수명을 분리한다.
class ENGINE_API IRenderContext
{
public:
	IRenderContext() = default;
	virtual ~IRenderContext() = default;

	IRenderContext(const IRenderContext&) = delete;
	IRenderContext& operator=(const IRenderContext&) = delete;
	IRenderContext(IRenderContext&&) = delete;
	IRenderContext& operator=(IRenderContext&&) = delete;

	virtual void Create(void* NativeWindowHandle) = 0;
	virtual void Release() = 0;

	virtual void Resize(uint32 Width, uint32 Height) = 0;
	virtual void BeginFrame(FCommandListHandle CommandList) = 0;
	virtual void EndFrame(FCommandListHandle CommandList) = 0;
	virtual void Present() = 0;

	virtual FRenderViewport GetViewport() const = 0;
};
