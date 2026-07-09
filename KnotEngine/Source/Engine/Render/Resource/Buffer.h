#pragma once

#include "Core/CoreTypes.h"
#include "Render/Resource/VertexTypes.h"

/*
class FVertexBuffer
{
public:
	FVertexBuffer() = default;
	~FVertexBuffer() { Release(); }

	FVertexBuffer(const FVertexBuffer&) = delete;
	FVertexBuffer& operator=(const FVertexBuffer&) = delete;
	FVertexBuffer(FVertexBuffer&&) noexcept;
	FVertexBuffer& operator=(FVertexBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InByteWidth, uint32 InStride);
	void Release();

	uint32 GetVertexCount() const { return VertexCount; }
	uint32 GetStride() const { return Stride; }

	ID3D11Buffer* GetBuffer() const;

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 VertexCount = 0;
	uint32 Stride = 0;
};

class FConstantBuffer
{
public:
	FConstantBuffer() = default;
	~FConstantBuffer() { Release(); }

	FConstantBuffer(const FConstantBuffer&) = delete;
	FConstantBuffer& operator=(const FConstantBuffer&) = delete;
	FConstantBuffer(FConstantBuffer&&) noexcept;
	FConstantBuffer& operator=(FConstantBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, uint32 InByteWidth);
	void Create(ID3D11Device* InDevice, uint32 InByteWidth, const char* DebugName);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth);

	ID3D11Buffer* GetBuffer();

private:
	ID3D11Buffer* Buffer = nullptr;
};

class FIndexBuffer
{
public:
	FIndexBuffer() = default;
	~FIndexBuffer() { Release(); }

	FIndexBuffer(const FIndexBuffer&) = delete;
	FIndexBuffer& operator=(const FIndexBuffer&) = delete;
	FIndexBuffer(FIndexBuffer&&) noexcept;
	FIndexBuffer& operator=(FIndexBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, const void* InData, uint32 InIndexCount, uint32 InByteWidth);
	void Release();

	uint32 GetIndexCount() const { return IndexCount; }
	ID3D11Buffer* GetBuffer() const;

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 IndexCount = 0;
};

class FMeshBuffer
{
public:
	FMeshBuffer() = default;
	~FMeshBuffer() { Release(); }

	FMeshBuffer(const FMeshBuffer&) = delete;
	FMeshBuffer& operator=(const FMeshBuffer&) = delete;
	FMeshBuffer(FMeshBuffer&&) = default;
	FMeshBuffer& operator=(FMeshBuffer&&) = default;

	template<typename T>
	void Create(ID3D11Device* InDevice, const TMeshData<T>& InMeshData);
	void Release();

	FVertexBuffer& GetVertexBuffer() { return VertexBuffer; }
	FIndexBuffer& GetIndexBuffer() { return IndexBuffer; }
	const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetIndexBuffer() const { return IndexBuffer; }
	bool IsValid() const { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }

private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer IndexBuffer;
};
*/