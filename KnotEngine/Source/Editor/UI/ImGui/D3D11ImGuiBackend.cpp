#include "UI/ImGui/D3D11ImGuiBackend.h"

#include "Core/Assert.h"
#include "Render/Backends/D3D11Backend.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

FD3D11ImGuiBackend::FD3D11ImGuiBackend(FD3D11Backend& InBackend)
    : Backend(InBackend)
{
}

void FD3D11ImGuiBackend::Startup()
{
	// null을 넘기면 ImGui 내부에서 터져 원인 지점이 백엔드 밖으로 밀려난다.
	check(Backend.GetNativeDevice());
	check(Backend.GetNativeContext());

	panicf(ImGui_ImplDX11_Init(Backend.GetNativeDevice(), Backend.GetNativeContext()), "ImGui DirectX 11 렌더 백엔드 초기화 실패.");
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
