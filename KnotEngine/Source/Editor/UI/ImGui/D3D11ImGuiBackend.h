#pragma once

#include "UI/ImGui/ImGuiRenderBackend.h"

class FD3D11RenderDevice;

class FD3D11ImGuiBackend final : public IImGuiRenderBackend
{
public:
	explicit FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice);

	void Startup() override;
	void BeginFrame() override;
	void Render(ImDrawData* DrawData) override;
	void Shutdown() override;

private:
	FD3D11RenderDevice& RenderDevice;
};
