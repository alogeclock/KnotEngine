#include "Render/D3D11/D3D11BufferPool.h"

#include "Core/Assert.h"

#include <d3d11.h>

#include <cstring>
#include <limits>
#include <utility>

FBufferHandle FD3D11BufferPool::CreateBuffer(ID3D11Device* Device, const FBufferDesc& Desc, std::span<const uint8> InitialData)
{
	panic(Device);
	panicf(Desc.Size > 0 && InitialData.size() <= Desc.Size,
	       "잘못된 Buffer 생성 요청. Size={}, InitialBytes={}", Desc.Size, InitialData.size());
	panicf(Desc.Access == EResourceAccess::CPUWrite || InitialData.size() == Desc.Size,
	       "GPUOnly Buffer는 전체 초기 데이터가 필요하다. Size={}, InitialBytes={}", Desc.Size, InitialData.size());

	D3D11_BUFFER_DESC NativeDesc = {};
	NativeDesc.ByteWidth = Desc.Size;
	switch (Desc.Usage)
	{
	case EBufferUsage::Vertex: NativeDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; break;
	case EBufferUsage::Index: NativeDesc.BindFlags = D3D11_BIND_INDEX_BUFFER; break;
	case EBufferUsage::Constant:
		panicf(Desc.Size % 16 == 0, "Constant Buffer 크기는 16바이트 정렬이어야 한다. Size={}", Desc.Size);
		NativeDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		break;
	}

	if (Desc.Access == EResourceAccess::CPUWrite)
	{
		NativeDesc.Usage = D3D11_USAGE_DYNAMIC;
		NativeDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}
	else
	{
		NativeDesc.Usage = D3D11_USAGE_IMMUTABLE;
	}

	D3D11_SUBRESOURCE_DATA NativeInitialData = {};
	NativeInitialData.pSysMem = InitialData.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	const HRESULT Result = Device->CreateBuffer(&NativeDesc, InitialData.empty() ? nullptr : &NativeInitialData, Buffer.GetAddressOf());
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	const FBufferHandle Handle = StoreBuffer(std::move(Buffer), Desc);
	panic(Handle.IsValid());
	return Handle;
}

void FD3D11BufferPool::UpdateBuffer(ID3D11DeviceContext* DeviceContext, FBufferHandle Handle, std::span<const uint8> Data)
{
	panic(DeviceContext);
	ID3D11Buffer* Buffer = ResolveBuffer(Handle);
	const FBufferDesc* Desc = ResolveDesc(Handle);
	checkf(Buffer && Desc, "유효하지 않은 Buffer 핸들. Index={}, Generation={}", Handle.Index, Handle.Generation);
	panicf(Desc->Access == EResourceAccess::CPUWrite && Data.size() == Desc->Size,
	       "CPUWrite Buffer 갱신 크기 불일치. Expected={}, Actual={}", Desc->Size, Data.size());

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	const HRESULT Result = DeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	panicf(SUCCEEDED(Result), "ID3D11DeviceContext::Map 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
	std::memcpy(Mapped.pData, Data.data(), Data.size());
	DeviceContext->Unmap(Buffer, 0);
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
		Slot.Desc = {};
		AdvanceGeneration(Slot);
		FreeBufferIndices.push_back(Handle.Index);
	}
	Handle.Reset();
}

void FD3D11BufferPool::Release()
{
	FreeBufferIndices.clear();
	FreeBufferIndices.reserve(BufferSlots.size());
	for (uint32 Index = 0; Index < BufferSlots.size(); ++Index)
	{
		FBufferSlot& Slot = BufferSlots[Index];
		Slot.Buffer.Reset();
		Slot.Desc = {};
		AdvanceGeneration(Slot);
		FreeBufferIndices.push_back(Index);
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

const FBufferDesc* FD3D11BufferPool::ResolveDesc(FBufferHandle Handle) const
{
	if (!Handle.IsValid() || Handle.Index >= BufferSlots.size())
	{
		return nullptr;
	}
	const FBufferSlot& Slot = BufferSlots[Handle.Index];
	return Slot.Generation == Handle.Generation && Slot.Buffer ? &Slot.Desc : nullptr;
}

FBufferHandle FD3D11BufferPool::StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer, const FBufferDesc& Desc)
{
	if (!FreeBufferIndices.empty())
	{
		const uint32 Index = FreeBufferIndices.back();
		FreeBufferIndices.pop_back();
		FBufferSlot& Slot = BufferSlots[Index];
		check(!Slot.Buffer);
		Slot.Buffer = std::move(Buffer);
		Slot.Desc = Desc;
		return { Index, Slot.Generation };
	}

	panicf(BufferSlots.size() < (std::numeric_limits<uint32>::max)(), "D3D11 Buffer 슬롯 수가 uint32 범위를 초과했다.");
	FBufferSlot& Slot = BufferSlots.emplace_back();
	Slot.Buffer = std::move(Buffer);
	Slot.Desc = Desc;
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
