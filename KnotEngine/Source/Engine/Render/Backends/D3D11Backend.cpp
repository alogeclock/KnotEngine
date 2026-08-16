#include "Render/Backends/D3D11Backend.h"

#include "Core/IO/Paths.h"
#include "Core/Math/Matrix.h"
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

namespace
{
	const char* GetSemanticName(FVertexSemantic Semantic)
	{
		switch (Semantic)
		{
		case FVertexSemantic::Position: return "POSITION";
		case FVertexSemantic::Normal: return "NORMAL";
		case FVertexSemantic::Tangent: return "TANGENT";
		case FVertexSemantic::Color: return "COLOR";
		case FVertexSemantic::TexCoord0: return "TEXCOORD";
		}
		return nullptr;
	}

	DXGI_FORMAT GetDXGIFormat(FVertexFormat Format)
	{
		switch (Format)
		{
		case FVertexFormat::Float1: return DXGI_FORMAT_R32_FLOAT;
		case FVertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
		case FVertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case FVertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case FVertexFormat::Half2: return DXGI_FORMAT_R16G16_FLOAT;
		case FVertexFormat::Half4: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case FVertexFormat::UInt8x4: return DXGI_FORMAT_R8G8B8A8_UINT;
		case FVertexFormat::UNorm8x4: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case FVertexFormat::SNorm8x4: return DXGI_FORMAT_R8G8B8A8_SNORM;
		case FVertexFormat::UInt16x2: return DXGI_FORMAT_R16G16_UINT;
		case FVertexFormat::UInt16x4: return DXGI_FORMAT_R16G16B16A16_UINT;
		case FVertexFormat::UNorm16x2: return DXGI_FORMAT_R16G16_UNORM;
		case FVertexFormat::UNorm16x4: return DXGI_FORMAT_R16G16B16A16_UNORM;
		case FVertexFormat::SNorm16x2: return DXGI_FORMAT_R16G16_SNORM;
    case FVertexFormat::SNorm16x4: return DXGI_FORMAT_R16G16B16A16_SNORM;
		case FVertexFormat::UInt32: return DXGI_FORMAT_R32_UINT;
		case FVertexFormat::UInt32x2: return DXGI_FORMAT_R32G32_UINT;
		case FVertexFormat::UInt32x3: return DXGI_FORMAT_R32G32B32_UINT;
		case FVertexFormat::UInt32x4: return DXGI_FORMAT_R32G32B32A32_UINT;
		}
		return DXGI_FORMAT_UNKNOWN;
	}
}

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
        if (!Device || !SimpleVertexShaderInputSignature || VertexLayout.Elements.empty() ||
            VertexLayout.Elements.size() > D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
        {
            return nullptr;
        }

        TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDescs;
        LayoutDescs.reserve(VertexLayout.Elements.size());
        for (const FVertexElement& Element : VertexLayout.Elements)
        {
            const char* SemanticName = GetSemanticName(Element.Semantic);
            const DXGI_FORMAT Format = GetDXGIFormat(Element.Format);
            if (!SemanticName || Format == DXGI_FORMAT_UNKNOWN)
            {
                return nullptr;
            }

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
        return SUCCEEDED(Result) ? SimpleInputLayout.Get() : nullptr;
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

bool FD3D11Backend::Create(void* NativeWindowHandle)
{
    Release();
    if (!NativeWindowHandle)
    {
        return false;
    }

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
    if (FAILED(Result))
    {
        Release();
        return false;
    }

    Impl->SwapChain->GetDesc(&SwapChainDesc);
    Impl->Viewport = { 0.0f, 0.0f,
        static_cast<float>(SwapChainDesc.BufferDesc.Width),
        static_cast<float>(SwapChainDesc.BufferDesc.Height), 0.0f, 1.0f };

    Result = Impl->SwapChain->GetBuffer(0, IID_PPV_ARGS(Impl->FrameBuffer.ReleaseAndGetAddressOf()));
    if (FAILED(Result) || FAILED(Impl->Device->CreateRenderTargetView(
        Impl->FrameBuffer.Get(), nullptr, Impl->FrameBufferRTV.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = static_cast<UINT>(Impl->Viewport.Width);
    DepthDesc.Height = static_cast<UINT>(Impl->Viewport.Height);
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(Impl->Device->CreateTexture2D(
        &DepthDesc, nullptr, Impl->DepthStencilBuffer.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC DepthViewDesc = {};
    DepthViewDesc.Format = DepthDesc.Format;
    DepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(Impl->Device->CreateDepthStencilView(
        Impl->DepthStencilBuffer.Get(), &DepthViewDesc,
        Impl->DepthStencilView.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC DepthStateDesc = {};
    DepthStateDesc.DepthEnable = TRUE;
    DepthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    DepthStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    if (FAILED(Impl->Device->CreateDepthStencilState(
        &DepthStateDesc, Impl->DepthStencilState.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    if (FAILED(Impl->Device->CreateRasterizerState(
        &RasterizerDesc, Impl->RasterizerState.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderCode;
    Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderCode;
    Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
    const FWString ShaderPath = FPaths::ShaderDir() + L"Common.hlsl";
    Result = D3DCompileFromFile(
        ShaderPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", 0, 0,
        VertexShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
    if (FAILED(Result) || FAILED(Impl->Device->CreateVertexShader(
        VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(), nullptr,
        Impl->SimpleVertexShader.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    Result = D3DGetInputSignatureBlob(
        VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(),
        Impl->SimpleVertexShaderInputSignature.ReleaseAndGetAddressOf());
    if (FAILED(Result))
    {
        Release();
        return false;
    }

    ErrorBlob.Reset();
    Result = D3DCompileFromFile(
        ShaderPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", 0, 0,
        PixelShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
    if (FAILED(Result) || FAILED(Impl->Device->CreatePixelShader(
        PixelShaderCode->GetBufferPointer(), PixelShaderCode->GetBufferSize(), nullptr,
        Impl->SimplePixelShader.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    D3D11_BUFFER_DESC ConstantDesc = {};
    ConstantDesc.ByteWidth = sizeof(FImpl::FConstants);
    ConstantDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ConstantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(Impl->Device->CreateBuffer(
        &ConstantDesc, nullptr, Impl->ConstantBuffer.ReleaseAndGetAddressOf())))
    {
        Release();
        return false;
    }

    return true;
}

void FD3D11Backend::Release()
{
    if (!Impl)
    {
        return;
    }
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
    if (!Impl->DeviceContext || !Impl->FrameBufferRTV)
    {
        return;
    }

    Impl->DeviceContext->ClearRenderTargetView(Impl->FrameBufferRTV.Get(), Impl->ClearColor);
    if (Impl->DepthStencilView)
    {
        Impl->DeviceContext->ClearDepthStencilView(
            Impl->DepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

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
    if (Impl->SwapChain)
    {
        Impl->SwapChain->Present(1, 0);
    }
}

FBufferHandle FD3D11Backend::CreateVertexBuffer(
    std::span<const uint8> Data, uint32 VertexCount, uint32 Stride)
{
    if (!Impl->Device || Data.empty() || VertexCount == 0 || Stride == 0 ||
        VertexCount > (std::numeric_limits<uint32>::max)() / Stride ||
        Data.size() != static_cast<size_t>(VertexCount) * Stride)
    {
        return {};
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = VertexCount * Stride;
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Data.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
    if (FAILED(Impl->Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf())))
    {
        return {};
    }
    return Impl->StoreBuffer(std::move(Buffer));
}

FBufferHandle FD3D11Backend::CreateIndexBuffer(std::span<const uint32> Indices)
{
    if (!Impl->Device || Indices.empty() ||
        Indices.size() > (std::numeric_limits<uint32>::max)() / sizeof(uint32))
    {
        return {};
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = static_cast<UINT>(Indices.size_bytes());
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Indices.data();

    Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
    if (FAILED(Impl->Device->CreateBuffer(&Desc, &InitialData, Buffer.GetAddressOf())))
    {
        return {};
    }
    return Impl->StoreBuffer(std::move(Buffer));
}

void FD3D11Backend::DestroyBuffer(FBufferHandle& Handle)
{
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
    if (!Impl->DeviceContext || !Impl->ConstantBuffer)
    {
        return;
    }

    FImpl::FConstants Constants = {};
    std::memcpy(Constants.ModelViewProjection, WorldViewProjection.M, sizeof(Constants.ModelViewProjection));
    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (SUCCEEDED(Impl->DeviceContext->Map(
        Impl->ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        std::memcpy(Mapped.pData, &Constants, sizeof(Constants));
        Impl->DeviceContext->Unmap(Impl->ConstantBuffer.Get(), 0);
    }
}

void FD3D11Backend::PrepareShader()
{
    if (!Impl->DeviceContext)
    {
        return;
    }
    Impl->DeviceContext->VSSetShader(Impl->SimpleVertexShader.Get(), nullptr, 0);
    Impl->DeviceContext->PSSetShader(Impl->SimplePixelShader.Get(), nullptr, 0);
    ID3D11Buffer* ConstantBuffer = Impl->ConstantBuffer.Get();
    Impl->DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
}

void FD3D11Backend::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
    if (!Impl->DeviceContext || !MeshBuffer.IsValid())
    {
        return;
    }

    ID3D11InputLayout* InputLayout = Impl->GetOrCreateInputLayout(MeshBuffer.GetLayout());
    ID3D11Buffer* VertexBuffer = Impl->ResolveBuffer(MeshBuffer.GetVertexBuffer().GetHandle());
    if (!InputLayout || !VertexBuffer)
    {
        return;
    }

    Impl->DeviceContext->IASetInputLayout(InputLayout);
    const UINT Stride = MeshBuffer.GetStride();
    const UINT Offset = 0;
    Impl->DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);

    if (MeshBuffer.GetIndexCount() > 0)
    {
        ID3D11Buffer* IndexBuffer = Impl->ResolveBuffer(MeshBuffer.GetIndexBuffer().GetHandle());
        if (!IndexBuffer)
        {
            return;
        }
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
