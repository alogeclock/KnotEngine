#pragma once

#include "Render/RHI/RenderTypes.h"

#include <span>
#include <vector>
#include <wrl/client.h>

struct ID3D11Buffer;
struct ID3D11Device;

// D3D11 버퍼를 생성 및 소유하고, 버퍼의 조회와 생성/파괴를 담당하는 객체.
class FD3D11BufferPool final
{
public:
	FBufferHandle CreateVertexBuffer(ID3D11Device* Device, std::span<const uint8> Data, uint32 VertexCount, uint32 Stride);
	FBufferHandle CreateIndexBuffer(ID3D11Device* Device, std::span<const uint32> Indices);
	void DestroyBuffer(FBufferHandle& Handle);
	void Release();

	ID3D11Buffer* ResolveBuffer(FBufferHandle Handle) const;

private:
	struct FBufferSlot
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		uint32 Generation = 1;
	};

	FBufferHandle StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer);
	static void AdvanceGeneration(FBufferSlot& Slot);

	std::vector<FBufferSlot> BufferSlots;
	std::vector<uint32> FreeBufferIndices;
};
