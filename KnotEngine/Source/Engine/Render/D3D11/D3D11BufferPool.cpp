#include "Render/D3D11/D3D11BufferPool.h"

#include "Core/Assert.h"

#include <d3d11.h>

#include <limits>
#include <utility>

FBufferHandle FD3D11BufferPool::CreateVertexBuffer(
    ID3D11Device* Device, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
	panic(Device);
	panicf(!Data.empty() && VertexCount > 0 && Stride > 0 &&
	           VertexCount <= (std::numeric_limits<uint32>::max)() / Stride &&
	           Data.size() == static_cast<size_t>(VertexCount) * Stride,
	       "잘못된 Vertex Buffer 생성 요청. Bytes={}, VertexCount={}, Stride={}",
	       Data.size(), VertexCount, Stride);

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = VertexCount * Stride;
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = Data.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	const HRESULT Result = Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf());
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Vertex) 실패. HRESULT=0x{:08X}",
	       static_cast<uint32>(Result));
	const FBufferHandle Handle = StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

FBufferHandle FD3D11BufferPool::CreateIndexBuffer(
    ID3D11Device* Device, std::span<const uint32> Indices)
{
	panic(Device);
	panicf(!Indices.empty() && Indices.size() <= (std::numeric_limits<uint32>::max)() / sizeof(uint32),
	       "잘못된 Index Buffer 생성 요청. IndexCount={}", Indices.size());

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = static_cast<UINT>(Indices.size_bytes());
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = Indices.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	const HRESULT Result = Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf());
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Index) 실패. HRESULT=0x{:08X}",
	       static_cast<uint32>(Result));
	const FBufferHandle Handle = StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

void FD3D11BufferPool::DestroyBuffer(FBufferHandle& Handle)
{
	checkf(!Handle.IsValid() || Handle.Index < BufferSlots.size(),
	       "슬롯 범위를 벗어난 Buffer 핸들. Index={}, SlotCount={}", Handle.Index, BufferSlots.size());

	if (!Handle.IsValid() || Handle.Index >= BufferSlots.size())
	{
		Handle.Reset();
		return;
	}

	FBufferSlot& Slot = BufferSlots[Handle.Index];
	if (Slot.Generation == Handle.Generation)
	{
		Slot.Buffer.Reset();
		AdvanceGeneration(Slot);
	}
	Handle.Reset();
}

void FD3D11BufferPool::Release()
{
	for (FBufferSlot& Slot : BufferSlots)
	{
		Slot.Buffer.Reset();
		AdvanceGeneration(Slot);
	}
}

ID3D11Buffer* FD3D11BufferPool::ResolveBuffer(FBufferHandle Handle) const
{
	if (!Handle.IsValid() || Handle.Index >= BufferSlots.size())
	{
		return nullptr;
	}

	const FBufferSlot& Slot = BufferSlots[Handle.Index];
	return Slot.Generation == Handle.Generation ? Slot.Buffer.Get() : nullptr;
}

FBufferHandle FD3D11BufferPool::StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer)
{
	for (uint32 Index = 0; Index < BufferSlots.size(); ++Index)
	{
		FBufferSlot& Slot = BufferSlots[Index];
		if (!Slot.Buffer)
		{
			Slot.Buffer = std::move(Buffer);
			return { Index, Slot.Generation };
		}
	}

	FBufferSlot& Slot = BufferSlots.emplace_back();
	Slot.Buffer = std::move(Buffer);
	return { static_cast<uint32>(BufferSlots.size() - 1), Slot.Generation };
}

void FD3D11BufferPool::AdvanceGeneration(FBufferSlot& Slot)
{
	++Slot.Generation;
	if (Slot.Generation == 0)
	{
		Slot.Generation = 1;
	}
}
