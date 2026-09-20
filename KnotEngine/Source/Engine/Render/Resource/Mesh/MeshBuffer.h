#pragma once

#include "EngineAPI.h"

#include "Render/Resource/Buffer.h"
#include "Render/Resource/RenderResource.h"
#include "Render/RHI/VertexLayout.h"

#include <span>

class IRenderDevice;

// GPU 생성 호출 동안만 유효한 비소유 업로드 뷰.
struct ENGINE_API FMeshDataView
{
	std::span<const uint8> VertexBytes;
	std::span<const uint32> Indices;
	const FVertexLayout* Layout = nullptr;
	uint32 VertexCount = 0;
};

// 하나의 Draw/DrawIndexed 호출에 필요한 GPU Mesh Buffer 묶음.
// FStaticMeshResource와 Renderer 내부 Geometry가 소유하며 빈 상태와 GPU resident 상태를 구분한다.
class ENGINE_API FMeshBuffer final : public FRenderResource
{
public:
	FMeshBuffer() = default;
	~FMeshBuffer() override;
	FMeshBuffer(FMeshBuffer&& Other) noexcept;
	FMeshBuffer& operator=(FMeshBuffer&& Other) noexcept;

	bool Initialize(IRenderDevice& RenderDevice, const FMeshDataView& InDataView);
	bool IsValid() const;

	const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetIndexBuffer() const { return IndexBuffer; }
	const FVertexLayout& GetLayout() const { return VertexLayout; }

	uint32 GetVertexCount() const { return VertexBuffer.GetVertexCount(); }
	uint32 GetIndexCount() const { return IndexBuffer.GetIndexCount(); }
	uint32 GetStride() const { return VertexLayout.Stride; }

protected:
	void OnRelease() override;

private:
	static bool Validate(const FMeshDataView& DataView);

private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer IndexBuffer;
	FVertexLayout VertexLayout;
};
