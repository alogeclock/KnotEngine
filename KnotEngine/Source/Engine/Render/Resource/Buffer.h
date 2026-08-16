#pragma once

#include "Core/CoreTypes.h"
#include "Render/RHI/RenderTypes.h"

class IRenderBackend;
class URenderer;

class FVertexBuffer
{
public:
	FVertexBuffer() = default;
	~FVertexBuffer();

	FVertexBuffer(const FVertexBuffer&) = delete;
	FVertexBuffer& operator=(const FVertexBuffer&) = delete;
	FVertexBuffer(FVertexBuffer&& Other) noexcept;
	FVertexBuffer& operator=(FVertexBuffer&& Other) noexcept;

	void Release();

	bool IsValid() const { return Handle.IsValid() && VertexCount > 0 && Stride > 0; }
	FBufferHandle GetHandle() const { return Handle; }
	uint32 GetVertexCount() const { return VertexCount; }
	uint32 GetStride() const { return Stride; }

private:
	friend class URenderer;

	void Adopt(IRenderBackend& InOwner, FBufferHandle InHandle, uint32 InVertexCount, uint32 InStride);

	IRenderBackend* Owner = nullptr;
	FBufferHandle Handle;
	uint32 VertexCount = 0;
	uint32 Stride = 0;
};

class FIndexBuffer
{
public:
	FIndexBuffer() = default;
	~FIndexBuffer();

	FIndexBuffer(const FIndexBuffer&) = delete;
	FIndexBuffer& operator=(const FIndexBuffer&) = delete;
	FIndexBuffer(FIndexBuffer&& Other) noexcept;
	FIndexBuffer& operator=(FIndexBuffer&& Other) noexcept;

	void Release();

	bool IsValid() const { return Handle.IsValid() && IndexCount > 0; }
	FBufferHandle GetHandle() const { return Handle; }
	uint32 GetIndexCount() const { return IndexCount; }

private:
	friend class URenderer;

	void Adopt(IRenderBackend& InOwner, FBufferHandle InHandle, uint32 InIndexCount);

	IRenderBackend* Owner = nullptr;
	FBufferHandle Handle;
	uint32 IndexCount = 0;
};
