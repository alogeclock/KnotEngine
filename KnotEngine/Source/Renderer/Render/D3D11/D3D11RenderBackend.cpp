#include "Render/D3D11/D3D11RenderBackend.h"

FD3D11RenderBackend::FD3D11RenderBackend()
	: RenderContext(RenderDevice), ImGuiBackend(RenderDevice)
{
}
