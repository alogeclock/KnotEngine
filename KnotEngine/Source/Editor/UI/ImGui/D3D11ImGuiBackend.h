#pragma once

#include "UI/ImGui/ImGuiRenderBackend.h"

class FD3D11Backend;

class FD3D11ImGuiBackend final : public IImGuiRenderBackend
{
public:
	explicit FD3D11ImGuiBackend(FD3D11Backend& InBackend);

	void Startup() override;
	void BeginFrame() override;
	void Render(ImDrawData* DrawData) override;
	void Shutdown() override;

private:
	FD3D11Backend& Backend;
};
