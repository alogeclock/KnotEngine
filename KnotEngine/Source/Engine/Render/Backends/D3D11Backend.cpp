#include "Render/Backends/D3D11Backend.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/Math/Matrix.h"
#include "Render/Backends/D3DCommon.h"
#include "Render/Resource/MeshResources.h"
#include "Render/Resource/VertexLayouts.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct FD3D11Backend::FImpl
{
	struct FConstants
	{
		alignas(16) float ModelViewProjection[4][4];
	};

	struct FBufferSlot
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		uint32 Generation = 1;
	};

	ID3D11Buffer* ResolveBuffer(FBufferHandle Handle) const
	{
		if (!Handle.IsValid() || Handle.Index >= BufferSlots.size())
		{
			return nullptr;
		}
		const FBufferSlot& Slot = BufferSlots[Handle.Index];
		return Slot.Generation == Handle.Generation ? Slot.Buffer.Get() : nullptr;
	}

	FBufferHandle StoreBuffer(Microsoft::WRL::ComPtr<ID3D11Buffer>&& Buffer)
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

	ID3D11InputLayout* GetOrCreateInputLayout(const FVertexLayout& VertexLayout)
	{
		if (SimpleInputLayout)
		{
			return SimpleInputLayout.Get();
		}
		panic(Device);
		panic(SimpleVertexShaderInputSignature);
		panicf(!VertexLayout.Elements.empty() &&
				   VertexLayout.Elements.size() <= D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT,
			   "FVertexLayout의 Element 개수가 유효하지 않다. Count={}, Max={}",
			   VertexLayout.Elements.size(), D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT);

		TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDescs;
		LayoutDescs.reserve(VertexLayout.Elements.size());
		for (const FVertexElement& Element : VertexLayout.Elements)
		{
			const char* SemanticName = GetSemanticName(Element.Semantic);
			const DXGI_FORMAT Format = GetDXGIFormat(Element.Format);
			panicf(SemanticName, "GetSemanticName()이 처리하지 않는 FVertexSemantic. Value={}",
				   static_cast<uint32>(Element.Semantic));
			panicf(Format != DXGI_FORMAT_UNKNOWN, "GetDXGIFormat()이 처리하지 않는 FVertexFormat. Value={}",
				   static_cast<uint32>(Element.Format));

			D3D11_INPUT_ELEMENT_DESC Desc = {};
			Desc.SemanticName = SemanticName;
			Desc.SemanticIndex = Element.SemanticIndex;
			Desc.Format = Format;
			Desc.AlignedByteOffset = Element.Offset;
			Desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			LayoutDescs.push_back(Desc);
		}

		const HRESULT Result = Device->CreateInputLayout(
			LayoutDescs.data(), static_cast<UINT>(LayoutDescs.size()),
			SimpleVertexShaderInputSignature->GetBufferPointer(),
			SimpleVertexShaderInputSignature->GetBufferSize(),
			SimpleInputLayout.ReleaseAndGetAddressOf());
		panicf(SUCCEEDED(Result) && SimpleInputLayout,
			   "ID3D11Device::CreateInputLayout 실패. HRESULT=0x{:08X} "
			   "(FVertexLayout이 Vertex Shader 입력 시그니처와 일치하는지 확인할 것)",
			   static_cast<uint32>(Result));
		return SimpleInputLayout.Get();
	}

	Microsoft::WRL::ComPtr<ID3D11Device> Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain> SwapChain;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> FrameBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> FrameBufferRTV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> SimpleVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> SimplePixelShader;
	Microsoft::WRL::ComPtr<ID3DBlob> SimpleVertexShaderInputSignature;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> SimpleInputLayout;
	std::vector<FBufferSlot> BufferSlots;
	D3D11_VIEWPORT Viewport = {};
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
};

FD3D11Backend::FD3D11Backend()
	: Impl(std::make_unique<FImpl>())
{
}

FD3D11Backend::~FD3D11Backend()
{
	Release();
}

void FD3D11Backend::Create(void* NativeWindowHandle)
{
	Release();
	checkf(NativeWindowHandle, "FD3D11Backend::Create에 전달된 NativeWindowHandle이 null이다.");

	const D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = static_cast<HWND>(NativeWindowHandle);
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
	HRESULT Result = D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
		FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
		&SwapChainDesc, Impl->SwapChain.ReleaseAndGetAddressOf(),
		Impl->Device.ReleaseAndGetAddressOf(), nullptr,
		Impl->DeviceContext.ReleaseAndGetAddressOf());
	if (FAILED(Result))
	{
		CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		Result = D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateFlags,
			FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
			&SwapChainDesc, Impl->SwapChain.ReleaseAndGetAddressOf(),
			Impl->Device.ReleaseAndGetAddressOf(), nullptr,
			Impl->DeviceContext.ReleaseAndGetAddressOf());
	}
	panicf(SUCCEEDED(Result), "D3D11CreateDeviceAndSwapChain 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Impl->SwapChain->GetDesc(&SwapChainDesc);
	Impl->Viewport = { 0.0f, 0.0f,
					   static_cast<float>(SwapChainDesc.BufferDesc.Width),
					   static_cast<float>(SwapChainDesc.BufferDesc.Height), 0.0f, 1.0f };

	Result = Impl->SwapChain->GetBuffer(0, IID_PPV_ARGS(Impl->FrameBuffer.ReleaseAndGetAddressOf()));
	panicf(SUCCEEDED(Result), "IDXGISwapChain::GetBuffer(0) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Result = Impl->Device->CreateRenderTargetView(
		Impl->FrameBuffer.Get(), nullptr, Impl->FrameBufferRTV.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRenderTargetView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = static_cast<UINT>(Impl->Viewport.Width);
	DepthDesc.Height = static_cast<UINT>(Impl->Viewport.Height);
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	Result = Impl->Device->CreateTexture2D(
		&DepthDesc, nullptr, Impl->DepthStencilBuffer.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateTexture2D(DepthStencil) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_DEPTH_STENCIL_VIEW_DESC DepthViewDesc = {};
	DepthViewDesc.Format = DepthDesc.Format;
	DepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	Result = Impl->Device->CreateDepthStencilView(
		Impl->DepthStencilBuffer.Get(), &DepthViewDesc,
		Impl->DepthStencilView.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilView 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_DEPTH_STENCIL_DESC DepthStateDesc = {};
	DepthStateDesc.DepthEnable = TRUE;
	DepthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
	Result = Impl->Device->CreateDepthStencilState(
		&DepthStateDesc, Impl->DepthStencilState.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;
	Result = Impl->Device->CreateRasterizerState(
		&RasterizerDesc, Impl->RasterizerState.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRasterizerState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderCode;
	Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderCode;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	const FWString ShaderPath = FPaths::ShaderDir() + L"Common.hlsl";
	Result = D3DCompileFromFile(
		ShaderPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", 0, 0,
		VertexShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
	panicf(
		SUCCEEDED(Result),
		"Vertex Shader 컴파일 실패. HRESULT=0x{:08X}\n{}",
		static_cast<uint32>(Result),
		GetShaderErrorText(ErrorBlob.Get()));

	Result = Impl->Device->CreateVertexShader(
		VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(), nullptr,
		Impl->SimpleVertexShader.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateVertexShader 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Result = D3DGetInputSignatureBlob(
		VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(),
		Impl->SimpleVertexShaderInputSignature.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "D3DGetInputSignatureBlob 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	ErrorBlob.Reset();
	Result = D3DCompileFromFile(
		ShaderPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", 0, 0,
		PixelShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
	panicf(
		SUCCEEDED(Result),
		"Pixel Shader 컴파일 실패. HRESULT=0x{:08X}\n{}",
		static_cast<uint32>(Result),
		GetShaderErrorText(ErrorBlob.Get()));

	Result = Impl->Device->CreatePixelShader(
		PixelShaderCode->GetBufferPointer(), PixelShaderCode->GetBufferSize(), nullptr,
		Impl->SimplePixelShader.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreatePixelShader 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_BUFFER_DESC ConstantDesc = {};
	ConstantDesc.ByteWidth = sizeof(FImpl::FConstants);
	ConstantDesc.Usage = D3D11_USAGE_DYNAMIC;
	ConstantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Result = Impl->Device->CreateBuffer(
		&ConstantDesc, nullptr, Impl->ConstantBuffer.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateBuffer(Constant) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}

void FD3D11Backend::Release()
{
	check(Impl);
	if (Impl->DeviceContext)
	{
		Impl->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		Impl->DeviceContext->Flush();
	}

	for (FImpl::FBufferSlot& Slot : Impl->BufferSlots)
	{
		Slot.Buffer.Reset();
		++Slot.Generation;
		if (Slot.Generation == 0)
		{
			Slot.Generation = 1;
		}
	}
	Impl->SimpleInputLayout.Reset();
	Impl->SimpleVertexShaderInputSignature.Reset();
	Impl->SimpleVertexShader.Reset();
	Impl->SimplePixelShader.Reset();
	Impl->ConstantBuffer.Reset();
	Impl->RasterizerState.Reset();
	Impl->DepthStencilState.Reset();
	Impl->DepthStencilView.Reset();
	Impl->DepthStencilBuffer.Reset();
	Impl->FrameBufferRTV.Reset();
	Impl->FrameBuffer.Reset();
	Impl->SwapChain.Reset();
	Impl->DeviceContext.Reset();
	Impl->Device.Reset();
	Impl->Viewport = {};
}

void FD3D11Backend::Prepare()
{
	// Create() 성공 이후에만 호출되어야 한다. null이면 초기화 순서 자체가 잘못된 것.
	panic(Impl->DeviceContext);
	panic(Impl->FrameBufferRTV);
	panic(Impl->DepthStencilView);

	Impl->DeviceContext->ClearRenderTargetView(Impl->FrameBufferRTV.Get(), Impl->ClearColor);
	Impl->DeviceContext->ClearDepthStencilView(Impl->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11RenderTargetView* RenderTarget = Impl->FrameBufferRTV.Get();
	Impl->DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Impl->DeviceContext->RSSetViewports(1, &Impl->Viewport);
	Impl->DeviceContext->RSSetState(Impl->RasterizerState.Get());
	Impl->DeviceContext->OMSetRenderTargets(1, &RenderTarget, Impl->DepthStencilView.Get());
	Impl->DeviceContext->OMSetDepthStencilState(Impl->DepthStencilState.Get(), 0);
	Impl->DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void FD3D11Backend::SwapBuffer()
{
	panic(Impl->SwapChain);
	Impl->SwapChain->Present(1, 0);
}

FBufferHandle FD3D11Backend::CreateVertexBuffer(
	std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
	// 크기 계약이 깨진 채로 진행하면 pSysMem을 ByteWidth만큼 읽어 overread가 난다.
	panic(Impl->Device);
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
	const HRESULT Result = Impl->Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf());
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Vertex) 실패. HRESULT=0x{:08X}",
		   static_cast<uint32>(Result));
	const FBufferHandle Handle = Impl->StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

FBufferHandle FD3D11Backend::CreateIndexBuffer(std::span<const uint32> Indices)
{
	panic(Impl->Device);
	panicf(!Indices.empty() && Indices.size() <= (std::numeric_limits<uint32>::max)() / sizeof(uint32),
		   "잘못된 Index Buffer 생성 요청. IndexCount={}", Indices.size());

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = static_cast<UINT>(Indices.size_bytes());
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = Indices.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	const HRESULT Result = Impl->Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf());
	panicf(SUCCEEDED(Result) && Buffer, "ID3D11Device::CreateBuffer(Index) 실패. HRESULT=0x{:08X}",
		   static_cast<uint32>(Result));
	const FBufferHandle Handle = Impl->StoreBuffer(std::move(Buffer));
	panic(Handle.IsValid());
	return Handle;
}

void FD3D11Backend::DestroyBuffer(FBufferHandle& Handle)
{
	// 유효한 핸들인데 슬롯 범위를 벗어났다면 다른 백엔드의 핸들이 섞여 들어온 것이다.
	checkf(!Handle.IsValid() || Handle.Index < Impl->BufferSlots.size(),
		   "슬롯 범위를 벗어난 Buffer 핸들. Index={}, SlotCount={}", Handle.Index, Impl->BufferSlots.size());

	if (!Handle.IsValid() || Handle.Index >= Impl->BufferSlots.size())
	{
		Handle.Reset();
		return;
	}

	FImpl::FBufferSlot& Slot = Impl->BufferSlots[Handle.Index];
	if (Slot.Generation == Handle.Generation)
	{
		Slot.Buffer.Reset();
		++Slot.Generation;
		if (Slot.Generation == 0)
		{
			Slot.Generation = 1;
		}
	}
	Handle.Reset();
}

void FD3D11Backend::UpdateConstant(const FMatrix& WorldViewProjection)
{
	panic(Impl->DeviceContext);
	panic(Impl->ConstantBuffer);

	FImpl::FConstants Constants = {};
	std::memcpy(Constants.ModelViewProjection, WorldViewProjection.M, sizeof(Constants.ModelViewProjection));
	D3D11_MAPPED_SUBRESOURCE Mapped = {};

	// Map은 Device Removed 등 런타임 사유로 실패할 수 있다. 불변식 위반이 아니므로 로그로 다룬다.
	const HRESULT Result = Impl->DeviceContext->Map(
		Impl->ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	if (FAILED(Result))
	{
		KE_LOG_ONCE(LogD3D11, Error, "ID3D11DeviceContext::Map(ConstantBuffer) 실패. HRESULT=0x{:08X}",
					static_cast<uint32>(Result));
	}
	else
	{
		std::memcpy(Mapped.pData, &Constants, sizeof(Constants));
		Impl->DeviceContext->Unmap(Impl->ConstantBuffer.Get(), 0);
	}
}

void FD3D11Backend::PrepareShader()
{
	panic(Impl->DeviceContext);
	panic(Impl->SimpleVertexShader);
	panic(Impl->SimplePixelShader);
	panic(Impl->ConstantBuffer);

	Impl->DeviceContext->VSSetShader(Impl->SimpleVertexShader.Get(), nullptr, 0);
	Impl->DeviceContext->PSSetShader(Impl->SimplePixelShader.Get(), nullptr, 0);
	ID3D11Buffer* ConstantBuffer = Impl->ConstantBuffer.Get();
	Impl->DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
}

void FD3D11Backend::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
	panic(Impl->DeviceContext);
	panicf(MeshBuffer.IsValid(), "유효하지 않은 FMeshBuffer가 DrawMeshBuffer로 전달되었다.");

	ID3D11InputLayout* InputLayout = Impl->GetOrCreateInputLayout(MeshBuffer.GetLayout());
	ID3D11Buffer* VertexBuffer = Impl->ResolveBuffer(MeshBuffer.GetVertexBuffer().GetHandle());

	panic(InputLayout);
	panicf(VertexBuffer, "Vertex Buffer 핸들 해석 실패. Index={}, Generation={} (이미 파괴된 핸들)",
		   MeshBuffer.GetVertexBuffer().GetHandle().Index,
		   MeshBuffer.GetVertexBuffer().GetHandle().Generation);

	Impl->DeviceContext->IASetInputLayout(InputLayout);
	const UINT Stride = MeshBuffer.GetStride();
	const UINT Offset = 0;
	Impl->DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

	if (MeshBuffer.GetIndexCount() > 0)
	{
		ID3D11Buffer* IndexBuffer = Impl->ResolveBuffer(MeshBuffer.GetIndexBuffer().GetHandle());
		panicf(IndexBuffer, "Index Buffer 핸들 해석 실패. Index={}, Generation={} (이미 파괴된 핸들)",
			   MeshBuffer.GetIndexBuffer().GetHandle().Index,
			   MeshBuffer.GetIndexBuffer().GetHandle().Generation);

		Impl->DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		Impl->DeviceContext->DrawIndexed(MeshBuffer.GetIndexCount(), 0, 0);
		return;
	}

	Impl->DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	Impl->DeviceContext->Draw(MeshBuffer.GetVertexCount(), 0);
}

FRenderViewport FD3D11Backend::GetViewport() const
{
	return { Impl->Viewport.TopLeftX, Impl->Viewport.TopLeftY,
			 Impl->Viewport.Width, Impl->Viewport.Height,
			 Impl->Viewport.MinDepth, Impl->Viewport.MaxDepth };
}

ID3D11Device* FD3D11Backend::GetNativeDevice() const
{
	return Impl->Device.Get();
}

ID3D11DeviceContext* FD3D11Backend::GetNativeContext() const
{
	return Impl->DeviceContext.Get();
}
