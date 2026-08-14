#pragma once

#include "Render/Resource/MeshResources.h"

struct FMatrix;

// DirectX 11을 이용한 렌더링 시스템을 담당하는 클래스
class URenderer
{
public:
    URenderer() = default;
    URenderer(const URenderer&) = delete;
    URenderer& operator=(const URenderer&) = delete;
    URenderer(URenderer&&) = delete;
    URenderer& operator=(URenderer&&) = delete;
    ~URenderer() { Release(); }

    void Create(HWND hWindow);
    void Release();

    void Prepare();
    void SwapBuffer();

    void CreateShader();
    void ReleaseShader();
    
    void CreateConstantBuffer();
    void ReleaseConstantBuffer();
    void UpdateConstant(const FMatrix& WorldViewProjection);

    void PrepareShader();
    void DrawMeshBuffer(const FMeshBuffer& MeshBuffer);

    ID3D11Device* GetDevice() const { return Device; }
    ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	D3D11_VIEWPORT GetViewportInfo() const { return ViewportInfo; }

private:
    static const char* GetSemanticName(FVertexSemantic Semantic);
    static DXGI_FORMAT GetDXGIFormat(FVertexFormat Format);

    // 내부 초기화 메서드
    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();

    ID3D11InputLayout* GetOrCreateInputLayout(const FVertexLayout& VertexLayout);
    
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();

    void CreateDepthStencilBuffer();
    void ReleaseDepthStencilBuffer();
    
    void CreateRasterizerState();
    void ReleaseRasterizerState();

private:
    // 상수 버퍼 구조체
    struct FConstants
    {
        alignas(16) float ModelViewProjection[4][4];
    };

    // D3D 기본 객체
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // 렌더링 타겟 및 상태
    ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11RenderTargetView* FrameBufferRTV = nullptr;
    ID3D11Texture2D* DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    ID3D11DepthStencilState* DepthStencilState = nullptr;
    ID3D11RasterizerState* RasterizerState = nullptr;
    FConstantBuffer ConstantBuffer;

    // 셰이더 리소스
    ID3D11VertexShader* SimpleVertexShader = nullptr;
    ID3D11PixelShader* SimplePixelShader = nullptr;
    ID3DBlob* SimpleVertexShaderInputSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> SimpleInputLayout;

    // 설정 데이터
    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    D3D11_VIEWPORT ViewportInfo{};
};
