#pragma once

#include "VertexLayouts.h"

/**
 * URenderer
 * DirectX 11을 이용한 렌더링 시스템을 담당하는 클래스
 */
class URenderer
{
public:
    URenderer() = default;
    ~URenderer() { Release(); }

    // 인터페이스: 초기화 및 해제
    void Create(HWND hWindow);
    void Release();

    // 인터페이스: 프레임 제어
    void Prepare();
    void SwapBuffer();

    // 인터페이스: 셰이더 및 리소스 관리
    void CreateShader();
    void ReleaseShader();
    
    void CreateConstantBuffer();
    void ReleaseConstantBuffer();
    void UpdateConstant(FVector Offset, float Scale);

    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
    void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

    // 인터페이스: 드로우 콜
    void PrepareShader();
    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);

    ID3D11Device* GetDevice() const { return Device; }
    ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }

private:
    // 내부 초기화 메서드 (SRP 준수)
    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();
    
    void CreateRasterizerState();
    void ReleaseRasterizerState();

private:
    // 상수 버퍼 구조체
    struct FConstants
    {
        FVector Offset;
        float Scale;
    };

    // D3D 기본 객체
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

    // 렌더링 타겟 및 상태
    ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11RenderTargetView* FrameBufferRTV = nullptr;
    ID3D11RasterizerState* RasterizerState = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;

    // 셰이더 리소스
    ID3D11VertexShader* SimpleVertexShader = nullptr;
    ID3D11PixelShader* SimplePixelShader = nullptr;
    ID3D11InputLayout* SimpleInputLayout = nullptr;

    // 설정 데이터
    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    D3D11_VIEWPORT ViewportInfo{};
    unsigned int Stride = 0;
};
