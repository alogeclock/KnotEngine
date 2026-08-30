#include "UI/ImGui/D3D11ImGuiBackend.h"

#include "Core/Assert.h"
#include "Render/D3D11/D3D11RenderDevice.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

FD3D11ImGuiBackend::FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

void FD3D11ImGuiBackend::Startup()
{
	// null을 넘기면 ImGui 내부에서 터져 원인 지점이 백엔드 밖으로 밀려난다.
	check(RenderDevice.GetNativeDevice());
	check(RenderDevice.GetNativeContext());

	panicf(ImGui_ImplDX11_Init(RenderDevice.GetNativeDevice(), RenderDevice.GetNativeContext()), "ImGui DirectX 11 렌더 백엔드 초기화 실패.");
}

void FD3D11ImGuiBackend::BeginFrame()
{
	ImGui_ImplDX11_NewFrame();
}

void FD3D11ImGuiBackend::Render(ImDrawData* DrawData)
{
	check(DrawData);
	ImGui_ImplDX11_RenderDrawData(DrawData);
}

void FD3D11ImGuiBackend::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
}
