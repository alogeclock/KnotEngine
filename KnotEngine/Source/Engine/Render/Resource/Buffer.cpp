#include "Render/Resource/Buffer.h"

#include "Core/Assert.h"
#include "Render/RHI/RenderBackend.h"

#include <utility>

FVertexBuffer::~FVertexBuffer()
{
	Release();
}

FVertexBuffer::FVertexBuffer(FVertexBuffer&& Other) noexcept
    : Owner(std::exchange(Other.Owner, nullptr)), Handle(std::exchange(Other.Handle, {})), VertexCount(std::exchange(Other.VertexCount, 0)), Stride(std::exchange(Other.Stride, 0))
{
}

FVertexBuffer& FVertexBuffer::operator=(FVertexBuffer&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		Owner = std::exchange(Other.Owner, nullptr);
		Handle = std::exchange(Other.Handle, {});
		VertexCount = std::exchange(Other.VertexCount, 0);
		Stride = std::exchange(Other.Stride, 0);
	}
	return *this;
}

// 백엔드가 생성한 GPU 리소스 핸들의 소유권을 버퍼 객체가 넘겨받아 수명을 관리한다.
void FVertexBuffer::Adopt(IRenderBackend& InOwner, FBufferHandle InHandle, uint32 InVertexCount, uint32 InStride)
{
	checkf(InHandle.IsValid(), "유효하지 않은 핸들을 FVertexBuffer가 넘겨받았다.");
	checkf(InVertexCount > 0 && InStride > 0, "VertexCount={}, Stride={}", InVertexCount, InStride);

	Release();
	Owner = &InOwner;
	Handle = InHandle;
	VertexCount = InVertexCount;
	Stride = InStride;
}

void FVertexBuffer::Release()
{
	// 소유자 없이 유효한 핸들이 남아 있으면 GPU 버퍼가 조용히 누수된다.
	checkf(!Handle.IsValid() || Owner, "소유자가 없는 Vertex Buffer 핸들. Index={}", Handle.Index);

	if (Owner && Handle.IsValid())
	{
		Owner->DestroyBuffer(Handle);
	}
	Owner = nullptr;
	Handle.Reset();
	VertexCount = 0;
	Stride = 0;
}

FIndexBuffer::~FIndexBuffer()
{
	Release();
}

FIndexBuffer::FIndexBuffer(FIndexBuffer&& Other) noexcept
    : Owner(std::exchange(Other.Owner, nullptr)), Handle(std::exchange(Other.Handle, {})), IndexCount(std::exchange(Other.IndexCount, 0))
{
}

FIndexBuffer& FIndexBuffer::operator=(FIndexBuffer&& Other) noexcept
{
	if (this != &Other)
	{
		Release();
		Owner = std::exchange(Other.Owner, nullptr);
		Handle = std::exchange(Other.Handle, {});
		IndexCount = std::exchange(Other.IndexCount, 0);
	}
	return *this;
}

// 백엔드가 생성한 GPU 리소스 핸들의 소유권을 버퍼 객체가 넘겨받아 수명을 관리한다.
void FIndexBuffer::Adopt(IRenderBackend& InOwner, FBufferHandle InHandle, uint32 InIndexCount)
{
	checkf(InHandle.IsValid(), "유효하지 않은 핸들을 FIndexBuffer가 넘겨받았다.");
	checkf(InIndexCount > 0, "IndexCount={}", InIndexCount);

	Release();
	Owner = &InOwner;
	Handle = InHandle;
	IndexCount = InIndexCount;
}

void FIndexBuffer::Release()
{
	// 소유자 없이 유효한 핸들이 남아 있으면 GPU 버퍼가 조용히 누수된다.
	checkf(!Handle.IsValid() || Owner, "소유자가 없는 Index Buffer 핸들. Index={}", Handle.Index);

	if (Owner && Handle.IsValid())
	{
		Owner->DestroyBuffer(Handle);
	}
	Owner = nullptr;
	Handle.Reset();
	IndexCount = 0;
}
