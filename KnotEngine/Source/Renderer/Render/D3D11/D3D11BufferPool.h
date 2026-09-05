#pragma once

#include "Render/RHI/RenderTypes.h"

#include <span>
#include <vector>
#include <wrl/client.h>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;

class FD3D11BufferPool final
{
public:
	FBufferHandle CreateBuffer(ID3D11Device* Device, const FBufferDesc& Desc, std::span<const uint8> InitialData);
	void UpdateBuffer(ID3D11DeviceContext* DeviceContext, FBufferHandle Handle, std::span<const uint8> Data);
	void DestroyBuffer(FBufferHandle& Handle);
	void Release();

	ID3D11Buffer* ResolveBuffer(FBufferHandle Handle) const;
	const FBufferDesc* ResolveDesc(FBufferHandle Handle) const;

private:
	struct FBufferSlot
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		FBufferDesc Desc;
		uint32 Generation = 1;
	};

	FBufferHandle StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer, const FBufferDesc& Desc);
	static void AdvanceGeneration(FBufferSlot& Slot);

	std::vector<FBufferSlot> BufferSlots;
	std::vector<uint32> FreeBufferIndices;
};
