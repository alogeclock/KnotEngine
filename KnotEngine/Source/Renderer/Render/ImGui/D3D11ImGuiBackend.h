#pragma once

#include "RendererAPI.h"

#include "Render/ImGui/ImGuiRenderBackend.h"

class FD3D11RenderDevice;

class RENDERER_API FD3D11ImGuiBackend final : public IImGuiRenderBackend
{
public:
	explicit FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice);

	void Startup(ImGuiContext* Context) override;
	void BeginFrame() override;
	void Render(FCommandListHandle CommandList, ImDrawData* DrawData) override;
	void Shutdown() override;

private:
	FD3D11RenderDevice& RenderDevice;
};
