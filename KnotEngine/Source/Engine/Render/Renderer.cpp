#include "Render/Renderer.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Math/Matrix.h"
#include "Render/RHI/RenderContext.h"
#include "Render/RHI/RenderDevice.h"
#include "Render/Resource/Buffer.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/VertexTypes.h"

#include <limits>

URenderer::URenderer(IRenderDevice& InRenderDevice, IRenderContext& InRenderContext)
	: RenderDevice(InRenderDevice), RenderContext(InRenderContext)
{
}

URenderer::~URenderer()
{
	Release();
}

void URenderer::Create(void* NativeWindowHandle)
{
	Release();
	RenderDevice.Create();
	RenderContext.Create(NativeWindowHandle);

	const FWString ShaderPath = FPaths::ShaderDir() + L"Common.hlsl";
	VertexShader = RenderDevice.CreateShader({ ShaderPath, "VS", EShaderStage::Vertex });
	PixelShader = RenderDevice.CreateShader({ ShaderPath, "PS", EShaderStage::Pixel });
	GraphicsPipeline = RenderDevice.CreateGraphicsPipeline({
		VertexShader,
		PixelShader,
		FGeometryVertex::GetVertexLayout(),
		EPrimitiveTopology::TriangleList,
		true,
		true,
	});

	const FBufferDesc ConstantDesc = {
		static_cast<uint32>(sizeof(FMatrix)),
		EBufferUsage::Constant,
		EResourceAccess::CPUWrite,
	};
	FrameConstantBuffer = RenderDevice.CreateBuffer(ConstantDesc);
}

void URenderer::Release()
{
	checkf(!CommandList.IsValid(), "열린 Render Command List가 있는 상태에서 Renderer를 해제할 수 없다.");
	RenderDevice.DestroyBuffer(FrameConstantBuffer);
	RenderDevice.DestroyGraphicsPipeline(GraphicsPipeline);
	RenderDevice.DestroyShader(PixelShader);
	RenderDevice.DestroyShader(VertexShader);
	RenderContext.Release();
	RenderDevice.Release();
}

void URenderer::BeginFrame()
{
	checkf(!CommandList.IsValid(), "Renderer Frame이 이미 시작되었다.");
	CommandList = RenderDevice.BeginCommandList();
	RenderContext.BeginFrame(CommandList);
	RenderDevice.SetGraphicsPipeline(CommandList, GraphicsPipeline);
	RenderDevice.SetConstantBuffer(CommandList, EShaderStage::Vertex, 0, FrameConstantBuffer);
}

void URenderer::EndFrame()
{
	checkf(CommandList.IsValid(), "Renderer Frame이 시작되지 않았다.");
	RenderDevice.EndCommandList(CommandList);
	RenderDevice.Submit(CommandList);
	RenderContext.Present();
}

void URenderer::UpdateConstant(const FMatrix& WorldViewProjection)
{
	check(CommandList.IsValid());
	const auto* Bytes = reinterpret_cast<const uint8*>(&WorldViewProjection);
	RenderDevice.UpdateBuffer(FrameConstantBuffer, std::span<const uint8>(Bytes, sizeof(FMatrix)));
}

void URenderer::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
	check(CommandList.IsValid());
	checkf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 DrawMeshBuffer로 전달되었다.");
	checkf(MeshBuffer.GetLayout() == FGeometryVertex::GetVertexLayout(),
		"현재 Geometry Pipeline과 호환되지 않는 Vertex Layout이다.");

	RenderDevice.SetVertexBuffer(
		CommandList, MeshBuffer.GetVertexBuffer().GetHandle(), MeshBuffer.GetStride());
	if (MeshBuffer.GetIndexCount() > 0)
	{
		RenderDevice.SetIndexBuffer(
			CommandList, MeshBuffer.GetIndexBuffer().GetHandle(), EIndexFormat::UInt32);
		RenderDevice.DrawIndexed(CommandList, MeshBuffer.GetIndexCount());
	}
	else
	{
		RenderDevice.Draw(CommandList, MeshBuffer.GetVertexCount());
	}
}

FRenderViewport URenderer::GetViewport() const
{
	return RenderContext.GetViewport();
}

bool URenderer::CreateVertexBuffer(
	FVertexBuffer& OutVertexBuffer, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
	checkf(VertexCount > 0 && Stride > 0 && Data.size() == static_cast<size_t>(VertexCount) * Stride,
		"잘못된 Vertex Buffer 데이터. Bytes={}, VertexCount={}, Stride={}", Data.size(), VertexCount, Stride);
	checkf(Data.size() <= (std::numeric_limits<uint32>::max)(), "Vertex Buffer 크기가 uint32 범위를 초과했다. Bytes={}", Data.size());
	const FBufferDesc Desc = { static_cast<uint32>(Data.size()), EBufferUsage::Vertex, EResourceAccess::GPUOnly };
	FBufferHandle Handle = RenderDevice.CreateBuffer(Desc, Data);
	if (!Handle.IsValid())
	{
		return false;
	}

	OutVertexBuffer.Adopt(RenderDevice, Handle, VertexCount, Stride);
	return true;
}

bool URenderer::CreateIndexBuffer(FIndexBuffer& OutIndexBuffer, std::span<const uint32> Indices)
{
	checkf(!Indices.empty() && Indices.size_bytes() <= (std::numeric_limits<uint32>::max)(),
		"잘못된 Index Buffer 데이터. Count={}, Bytes={}", Indices.size(), Indices.size_bytes());
	const auto* Bytes = reinterpret_cast<const uint8*>(Indices.data());
	const std::span<const uint8> Data(Bytes, Indices.size_bytes());
	const FBufferDesc Desc = { static_cast<uint32>(Data.size()), EBufferUsage::Index, EResourceAccess::GPUOnly };
	FBufferHandle Handle = RenderDevice.CreateBuffer(Desc, Data);
	if (!Handle.IsValid())
	{
		return false;
	}

	OutIndexBuffer.Adopt(RenderDevice, Handle, static_cast<uint32>(Indices.size()));
	return true;
}
