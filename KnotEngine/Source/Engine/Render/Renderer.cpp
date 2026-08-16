#include "Renderer.h"

#include "Core/IO/Paths.h"
#include "Core/Math/Matrix.h"

#include <d3dcompiler.h>

// 정점 시맨틱에 대응하는 D3D 입력 이름을 반환합니다.
const char* URenderer::GetSemanticName(FVertexSemantic Semantic)
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

// 엔진 정점 포맷을 DXGI 포맷으로 변환합니다.
DXGI_FORMAT URenderer::GetDXGIFormat(FVertexFormat Format)
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

// 링커 옵션 또는 Pragma를 통해 라이브러리 연결
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

void URenderer::Create(HWND hWindow)
{
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
    CreateDepthStencilBuffer();
    CreateRasterizerState();
    CreateShader();
    CreateConstantBuffer();
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
    D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = 0;
    swapchaindesc.BufferDesc.Height = 0;
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchaindesc.SampleDesc.Count = 1;
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchaindesc.BufferCount = 2;
    swapchaindesc.OutputWindow = hWindow;
    swapchaindesc.Windowed = TRUE;
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);
    if (FAILED(hr))
    {
        createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            createDeviceFlags,
            featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
            &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);
    }
    if (FAILED(hr))
    {
        return;
    }

    if (SwapChain)
    {
        SwapChain->GetDesc(&swapchaindesc);
        ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
    }
}

void URenderer::CreateFrameBuffer()
{
    if (!SwapChain || !Device)
    {
        return;
    }

    HRESULT hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);
    if (FAILED(hr) || !FrameBuffer)
    {
        return;
    }

    hr = Device->CreateRenderTargetView(FrameBuffer, nullptr, &FrameBufferRTV);
    if (FAILED(hr))
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }
}

void URenderer::CreateDepthStencilBuffer()
{
    if (!Device || ViewportInfo.Width <= 0.0f || ViewportInfo.Height <= 0.0f)
    {
        return;
    }

    D3D11_TEXTURE2D_DESC depthStencilDesc = {};
    depthStencilDesc.Width = static_cast<UINT>(ViewportInfo.Width);
    depthStencilDesc.Height = static_cast<UINT>(ViewportInfo.Height);
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    if (FAILED(Device->CreateTexture2D(&depthStencilDesc, nullptr, &DepthStencilBuffer)))
    {
        return;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format = depthStencilDesc.Format;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    Device->CreateDepthStencilView(DepthStencilBuffer, &depthStencilViewDesc, &DepthStencilView);

    D3D11_DEPTH_STENCIL_DESC depthStencilStateDesc = {};
    depthStencilStateDesc.DepthEnable = TRUE;
    depthStencilStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilStateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilStateDesc.StencilEnable = FALSE;

    Device->CreateDepthStencilState(&depthStencilStateDesc, &DepthStencilState);
}

void URenderer::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID;
    rasterizerdesc.CullMode = D3D11_CULL_BACK;

    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void URenderer::CreateShader()
{
    ReleaseShader();

    if (!Device)
    {
        return;
    }

    ID3DBlob* VertexShaderCSO = nullptr;
    ID3DBlob* PixelShaderCSO = nullptr;
    ID3DBlob* ErrorBlob = nullptr;

    // 셰이더 컴파일
    const FWString ShaderPath = FPaths::ShaderDir() + L"Common.hlsl";
    HRESULT hr = D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", 0, 0, &VertexShaderCSO, &ErrorBlob);
    if (SUCCEEDED(hr) && VertexShaderCSO)
    {
        const HRESULT ShaderResult = Device->CreateVertexShader(
            VertexShaderCSO->GetBufferPointer(),
            VertexShaderCSO->GetBufferSize(),
            nullptr,
            &SimpleVertexShader);
        if (SUCCEEDED(ShaderResult))
        {
            ID3DBlob* InputSignature = nullptr;
            if (SUCCEEDED(D3DGetInputSignatureBlob(
                    VertexShaderCSO->GetBufferPointer(),
                    VertexShaderCSO->GetBufferSize(),
                    &InputSignature)))
            {
                SimpleVertexShaderInputSignature = InputSignature;
            }
        }
    }
    if (ErrorBlob)
    {
        ErrorBlob->Release();
        ErrorBlob = nullptr;
    }

    hr = D3DCompileFromFile(ShaderPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", 0, 0, &PixelShaderCSO, &ErrorBlob);
    if (SUCCEEDED(hr) && PixelShaderCSO)
    {
        Device->CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);
    }
    if (ErrorBlob)
    {
        ErrorBlob->Release();
        ErrorBlob = nullptr;
    }

    if (VertexShaderCSO)
    {
        VertexShaderCSO->Release();
    }
    if (PixelShaderCSO)
    {
        PixelShaderCSO->Release();
    }
}

void URenderer::CreateConstantBuffer()
{
    ConstantBuffer.Initialize(Device, sizeof(FConstants));
}

void URenderer::Prepare()
{
    if (!DeviceContext || !FrameBufferRTV)
    {
        return;
    }

    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    if (DepthStencilView)
    {
        DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
    DeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
    DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void URenderer::PrepareShader()
{
    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);

    if (ConstantBuffer.IsValid())
    {
        ID3D11Buffer* NativeConstantBuffer = ConstantBuffer.GetNativeBuffer();
        DeviceContext->VSSetConstantBuffers(0, 1, &NativeConstantBuffer);
    }
}

void URenderer::UpdateConstant(const FMatrix& WorldViewProjection)
{
    if (!DeviceContext || !ConstantBuffer.IsValid())
    {
        return;
    }

    FConstants Constants = {};
    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Column = 0; Column < 4; ++Column)
        {
            Constants.ModelViewProjection[Row][Column] = WorldViewProjection.M[Row][Column];
        }
    }
    ConstantBuffer.Update(DeviceContext, &Constants, sizeof(Constants));
}

void URenderer::DrawMeshBuffer(const FMeshBuffer& MeshBuffer)
{
    if (!DeviceContext || !MeshBuffer.IsValid())
    {
        return;
    }

    ID3D11InputLayout* InputLayout = GetOrCreateInputLayout(MeshBuffer.GetLayout());
    if (!InputLayout)
    {
        return;
    }
    DeviceContext->IASetInputLayout(InputLayout);

    const FVertexBuffer& VertexBuffer = MeshBuffer.GetVertexBuffer();
    ID3D11Buffer* NativeVertexBuffer = VertexBuffer.GetNativeBuffer();
    const UINT Stride = VertexBuffer.GetStride();
    const UINT Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &NativeVertexBuffer, &Stride, &Offset);

    if (MeshBuffer.GetIndexCount() > 0)
    {
        ID3D11Buffer* NativeIndexBuffer = MeshBuffer.GetIndexBuffer().GetNativeBuffer();
        DeviceContext->IASetIndexBuffer(NativeIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        DeviceContext->DrawIndexed(MeshBuffer.GetIndexCount(), 0, 0);
        return;
    }

    DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->Draw(MeshBuffer.GetVertexCount(), 0);
}

void URenderer::SwapBuffer()
{
    if (SwapChain)
    {
        SwapChain->Present(1, 0);
    }
}

void URenderer::Release()
{
    ReleaseShader();
    ReleaseConstantBuffer();
    ReleaseDepthStencilBuffer();
    ReleaseRasterizerState();

    if (DeviceContext)
    {
        DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();
}

void URenderer::ReleaseDeviceAndSwapChain()
{
    if (DeviceContext)
    {
        DeviceContext->Flush();
    }
    if (SwapChain)
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }
    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }
    if (DeviceContext)
    {
        DeviceContext->Release();
        DeviceContext = nullptr;
    }
}

void URenderer::ReleaseFrameBuffer()
{
    if (FrameBuffer)
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }
    if (FrameBufferRTV)
    {
        FrameBufferRTV->Release();
        FrameBufferRTV = nullptr;
    }
}

void URenderer::ReleaseDepthStencilBuffer()
{
    if (DepthStencilState)
    {
        DepthStencilState->Release();
        DepthStencilState = nullptr;
    }
    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
    if (DepthStencilBuffer)
    {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }
}

void URenderer::ReleaseRasterizerState()
{
    if (RasterizerState)
    {
        RasterizerState->Release();
        RasterizerState = nullptr;
    }
}

void URenderer::ReleaseShader()
{
    SimpleInputLayout.Reset();

    if (SimpleVertexShaderInputSignature)
    {
        SimpleVertexShaderInputSignature->Release();
        SimpleVertexShaderInputSignature = nullptr;
    }

    if (SimpleVertexShader)
    {
        SimpleVertexShader->Release();
        SimpleVertexShader = nullptr;
    }
    if (SimplePixelShader)
    {
        SimplePixelShader->Release();
        SimplePixelShader = nullptr;
    }
}

ID3D11InputLayout* URenderer::GetOrCreateInputLayout(const FVertexLayout& VertexLayout)
{
    if (SimpleInputLayout)
    {
        return SimpleInputLayout.Get();
    }

    if (!Device ||
        !SimpleVertexShaderInputSignature ||
        VertexLayout.Elements.empty() ||
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
        LayoutDescs.data(),
        static_cast<UINT>(LayoutDescs.size()),
        SimpleVertexShaderInputSignature->GetBufferPointer(),
        SimpleVertexShaderInputSignature->GetBufferSize(),
        SimpleInputLayout.GetAddressOf());
    if (FAILED(Result))
    {
        return nullptr;
    }

    return SimpleInputLayout.Get();
}

void URenderer::ReleaseConstantBuffer()
{
    ConstantBuffer.Release();
}
