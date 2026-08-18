#include "Render/D3D11/D3D11BufferPool.h"

#include "Core/Assert.h"

#include <d3d11.h>

#include <limits>
#include <utility>

FBufferHandle FD3D11BufferPool::CreateVertexBuffer(ID3D11Device* Device, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
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
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Vertex) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	// 생성한 D3D Buffer의 소유권을 Buffer Pool로 넘기고, 외부에서는 핸들로 접근하도록 한다.
	const FBufferHandle Handle = StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

FBufferHandle FD3D11BufferPool::CreateIndexBuffer(ID3D11Device* Device, std::span<const uint32> Indices)
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
	// 생성한 D3D Buffer의 소유권을 Buffer Pool로 넘기고, 외부에서는 핸들로 접근하도록 한다.
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Index) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	const FBufferHandle Handle = StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

// 핸들을 통해 버퍼를 파괴하되, 슬롯의 세대를 증가시켜 이전의 핸들로 참조하지 못하도록 한다.
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

// Buffer Handle을 통해 적절한 Buffer에 접근하고, 슬롯과 핸들의 세대가 다를 때 실패하도록 한다.
// 파괴된 슬롯을 다른 버퍼가 재사용할 때, 이전의 핸들로 접근할 수 없도록 방어한다.
ID3D11Buffer* FD3D11BufferPool::ResolveBuffer(FBufferHandle Handle) const
{
	if (!Handle.IsValid() || Handle.Index >= BufferSlots.size())
	{
		return nullptr;
	}

	const FBufferSlot& Slot = BufferSlots[Handle.Index];
	return Slot.Generation == Handle.Generation ? Slot.Buffer.Get() : nullptr;
}

// Buffer Pool에서 빈 슬롯을 찾고 버퍼를 저장하거나, 새로운 슬롯을 생성한다.
FBufferHandle FD3D11BufferPool::StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer)
{
	for (uint32 Index = 0; Index < BufferSlots.size(); ++Index)
	{
		FBufferSlot& Slot = BufferSlots[Index];
		if (!Slot.Buffer)
		{
			// StoreBuffer의 Buffer 매개변수에서 BufferSlots[Index].Buffer로 소유권이 이동한다.
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
