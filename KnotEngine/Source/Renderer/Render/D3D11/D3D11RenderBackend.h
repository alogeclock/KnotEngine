#pragma once

#include "RendererAPI.h"

#include "Render/RenderBackend.h"
#include "Render/D3D11/D3D11RenderContext.h"
#include "Render/D3D11/D3D11RenderDevice.h"
#include "Render/D3D11/D3D11ShaderFormat.h"
#include "Render/ImGui/D3D11ImGuiBackend.h"

// D3D11 Device와 이에 종속된 Context, Shader Compiler, ImGui Backend의 구체적인 소유자다.
class RENDERER_API FD3D11RenderBackend final : public IRenderBackend
{
public:
	FD3D11RenderBackend();

	IRenderDevice& GetRenderDevice() override { return RenderDevice; }
	IRenderContext& GetRenderContext() override { return RenderContext; }
	IShaderFormat& GetShaderFormat() override { return ShaderFormat; }
	IImGuiRenderBackend& GetImGuiRenderBackend() override { return ImGuiBackend; }

private:
	FD3D11RenderDevice RenderDevice;
	FD3D11RenderContext RenderContext;
	FD3D11ShaderFormat ShaderFormat;
	FD3D11ImGuiBackend ImGuiBackend;
};
