#include "Render/RenderBackend.h"

#include "Render/D3D11/D3D11RenderBackend.h"

std::unique_ptr<IRenderBackend> CreateRenderBackend()
{
	return std::make_unique<FD3D11RenderBackend>();
}
