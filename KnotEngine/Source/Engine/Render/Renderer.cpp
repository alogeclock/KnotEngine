#include "Render/Renderer.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderBackend.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/MeshResources.h"

URenderer::URenderer(IRenderBackend& InBackend)
	: Backend(InBackend)
{
}

URenderer::~URenderer()
{
	Release();
}

void URenderer::Create(void* NativeWindowHandle)
{
	Backend.Create(NativeWindowHandle);
}

void URenderer::Release()
{
	Backend.Release();
}

void URenderer::Prepare()
{
	Backend.Prepare();
}

void URenderer::SwapBuffer()
{
	Backend.SwapBuffer();
}

void URenderer::UpdateConstant(const FMatrix& WorldViewProjection)
{
	Backend.UpdateConstant(WorldViewProjection);
}

void URenderer::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
	Backend.DrawMeshBuffer(MeshBuffer);
}

FRenderViewport URenderer::GetViewport() const
{
	return Backend.GetViewport();
}

bool URenderer::CreateVertexBuffer(FVertexBuffer& OutVertexBuffer, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
	FBufferHandle Handle = Backend.CreateVertexBuffer(Data, VertexCount, Stride);
	if (!Handle.IsValid())
	{
		return false;
	}

	OutVertexBuffer.Adopt(Backend, Handle, VertexCount, Stride);
	return true;
}

bool URenderer::CreateIndexBuffer(FIndexBuffer& OutIndexBuffer, std::span<const uint32> Indices)
{
	FBufferHandle Handle = Backend.CreateIndexBuffer(Indices);
	if (!Handle.IsValid())
	{
		return false;
	}

	OutIndexBuffer.Adopt(Backend, Handle, static_cast<uint32>(Indices.size()));
	return true;
}
