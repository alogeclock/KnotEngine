#pragma once

#include "Core/CoreTypes.h"

#include <d3d11.h>
#include <wrl/client.h>

class FVertexBuffer
{
public:
    FVertexBuffer() = default;
    ~FVertexBuffer() { Release(); }

    FVertexBuffer(const FVertexBuffer&) = delete;
    FVertexBuffer& operator=(const FVertexBuffer&) = delete;
    FVertexBuffer(FVertexBuffer&& Other) noexcept;
    FVertexBuffer& operator=(FVertexBuffer&& Other) noexcept;

    bool Initialize(ID3D11Device* Device, const void* Data, uint32 VertexCount, uint32 Stride);
    void Release();

    bool IsValid() const { return Buffer != nullptr && VertexCount > 0 && Stride > 0; }
    ID3D11Buffer* GetNativeBuffer() const { return Buffer.Get(); }
    uint32 GetVertexCount() const { return VertexCount; }
    uint32 GetStride() const { return Stride; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
    uint32 VertexCount = 0;
    uint32 Stride = 0;
};

class FIndexBuffer
{
public:
    FIndexBuffer() = default;
    ~FIndexBuffer() { Release(); }

    FIndexBuffer(const FIndexBuffer&) = delete;
    FIndexBuffer& operator=(const FIndexBuffer&) = delete;
    FIndexBuffer(FIndexBuffer&& Other) noexcept;
    FIndexBuffer& operator=(FIndexBuffer&& Other) noexcept;

    bool Initialize(ID3D11Device* Device, const uint32* Data, uint32 IndexCount);
    void Release();

    bool IsValid() const { return Buffer != nullptr && IndexCount > 0; }
    ID3D11Buffer* GetNativeBuffer() const { return Buffer.Get(); }
    uint32 GetIndexCount() const { return IndexCount; }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
    uint32 IndexCount = 0;
};

class FConstantBuffer
{
public:
    FConstantBuffer() = default;
    ~FConstantBuffer() { Release(); }

    FConstantBuffer(const FConstantBuffer&) = delete;
    FConstantBuffer& operator=(const FConstantBuffer&) = delete;
    FConstantBuffer(FConstantBuffer&& Other) noexcept;
    FConstantBuffer& operator=(FConstantBuffer&& Other) noexcept;

    bool Initialize(ID3D11Device* Device, uint32 InDataSize);
    bool Update(ID3D11DeviceContext* Context, const void* Data, uint32 InDataSize);
    void Release();

    bool IsValid() const { return Buffer != nullptr; }
    uint32 GetDataSize() const { return DataSize; }
    uint32 GetByteWidth(uint32 InDataSize) const { return (InDataSize + Alignment - 1) & ~(Alignment - 1); }

    ID3D11Buffer* GetNativeBuffer() const { return Buffer.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	static constexpr uint32 Alignment = 16u;
    uint32 DataSize = 0; // CPU에서 전달되는 실제 데이터의 크기
};
