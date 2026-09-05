#include "Render/ImGui/D3D11ImGuiBackend.h"

#include "Core/Assert.h"
#include "Render/D3D11/D3D11RenderDevice.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>

FD3D11ImGuiBackend::FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

void FD3D11ImGuiBackend::Startup(ImGuiContext* Context)
{
	// vcpkg의 ImGui는 정적 라이브러리이므로 DLL별 현재 컨텍스트를 명시적으로 연결한다.
	panic(Context);
	ImGui::SetCurrentContext(Context);
	// null을 넘기면 ImGui 내부에서 터져 원인 지점이 백엔드 밖으로 밀려난다.
	check(RenderDevice.GetNativeDevice());
	check(RenderDevice.GetNativeContext());

	panicf(ImGui_ImplDX11_Init(RenderDevice.GetNativeDevice(), RenderDevice.GetNativeContext()), "ImGui DirectX 11 렌더 백엔드 초기화 실패.");
}

void FD3D11ImGuiBackend::BeginFrame()
{
	ImGui_ImplDX11_NewFrame();
}

void FD3D11ImGuiBackend::Render(FCommandListHandle CommandList, ImDrawData* DrawData)
{
	check(CommandList.IsValid());
	check(DrawData);
	ImGui_ImplDX11_RenderDrawData(DrawData);
}

void FD3D11ImGuiBackend::Shutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui::SetCurrentContext(nullptr);
}
