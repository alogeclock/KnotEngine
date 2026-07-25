#include "Render/Resource/Buffer.h"

#include <cstring>
#include <limits>
#include <utility>

FVertexBuffer::FVertexBuffer(FVertexBuffer&& Other) noexcept
    : Buffer(std::move(Other.Buffer))
    , VertexCount(std::exchange(Other.VertexCount, 0))
    , Stride(std::exchange(Other.Stride, 0))
{
}

FVertexBuffer& FVertexBuffer::operator=(FVertexBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = std::move(Other.Buffer);
        VertexCount = std::exchange(Other.VertexCount, 0);
        Stride = std::exchange(Other.Stride, 0);
    }

    return *this;
}

bool FVertexBuffer::Initialize(ID3D11Device* Device, const void* Data, uint32 InVertexCount, uint32 InStride)
{
    Release();

    if (!Device || !Data || InVertexCount == 0 || InStride == 0)
    {
        return false;
    }

    if (InVertexCount > (std::numeric_limits<uint32>::max)() / InStride)
    {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.ByteWidth = InVertexCount * InStride;
    BufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Data;

    if (FAILED(Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf())))
    {
        return false;
    }

    VertexCount = InVertexCount;
    Stride = InStride;
    return true;
}

void FVertexBuffer::Release()
{
    Buffer.Reset();
    VertexCount = 0;
    Stride = 0;
}

FIndexBuffer::FIndexBuffer(FIndexBuffer&& Other) noexcept
    : Buffer(std::move(Other.Buffer))
    , IndexCount(std::exchange(Other.IndexCount, 0))
{
}

FIndexBuffer& FIndexBuffer::operator=(FIndexBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = std::move(Other.Buffer);
        IndexCount = std::exchange(Other.IndexCount, 0);
    }

    return *this;
}

bool FIndexBuffer::Initialize(ID3D11Device* Device, const uint32* Data, uint32 InIndexCount)
{
    Release();

    if (!Device || !Data || InIndexCount == 0)
    {
        return false;
    }

    if (InIndexCount > (std::numeric_limits<uint32>::max)() / sizeof(uint32))
    {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.ByteWidth = InIndexCount * sizeof(uint32);
    BufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Data;

    if (FAILED(Device->CreateBuffer(&BufferDesc, &InitialData, Buffer.GetAddressOf())))
    {
        return false;
    }

    IndexCount = InIndexCount;
    return true;
}

void FIndexBuffer::Release()
{
    Buffer.Reset();
    IndexCount = 0;
}

FConstantBuffer::FConstantBuffer(FConstantBuffer&& Other) noexcept
    : Buffer(std::move(Other.Buffer))
    , DataSize(std::exchange(Other.DataSize, 0))
{
}

FConstantBuffer& FConstantBuffer::operator=(FConstantBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = std::move(Other.Buffer);
        DataSize = std::exchange(Other.DataSize, 0);
    }

    return *this;
}

bool FConstantBuffer::Initialize(ID3D11Device* Device, uint32 InDataSize)
{
    Release();

    constexpr uint32 MaxByteWidth = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;

    if (!Device || InDataSize == 0 || InDataSize > MaxByteWidth)
    {
        return false;
    }

    D3D11_BUFFER_DESC BufferDesc = {};
    BufferDesc.ByteWidth = GetByteWidth(InDataSize);
    BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(Device->CreateBuffer(&BufferDesc, nullptr, Buffer.GetAddressOf())))
    {
        return false;
    }

    DataSize = InDataSize;
    return true;
}

bool FConstantBuffer::Update(ID3D11DeviceContext* Context, const void* Data, uint32 InDataSize)
{
    if (!Context || !Data || !IsValid() || InDataSize == 0 || InDataSize > DataSize)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};
    if (FAILED(Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
    {
        return false;
    }

    std::memset(MappedResource.pData, 0, GetByteWidth(DataSize));
    std::memcpy(MappedResource.pData, Data, InDataSize);
    Context->Unmap(Buffer.Get(), 0);
    return true;
}

void FConstantBuffer::Release()
{
    Buffer.Reset();
    DataSize = 0;
}
