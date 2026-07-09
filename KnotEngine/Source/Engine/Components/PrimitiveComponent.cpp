#include "Components/PrimitiveComponent.h"

UPrimitiveComponent::~UPrimitiveComponent()
{
	ReleaseVertexBuffer();
}

void UPrimitiveComponent::ReleaseVertexBuffer()
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}

	VertexCount = 0;
}

void UPrimitiveComponent::Render(float DeltaTime, URenderer& Renderer)
{
	if (VertexBuffer == nullptr || VertexCount == 0)
	{
		return;
	}

	Rotation += DeltaTime;

	D3D11_VIEWPORT ViewportInfo = Renderer.GetViewportInfo();
	const float aspectRatio = ViewportInfo.Height > 0.0f ? ViewportInfo.Width / ViewportInfo.Height : 1.0f;

	const DirectX::XMMATRIX world = DirectX::XMMatrixRotationRollPitchYaw(Rotation * 0.65f, Rotation, Rotation * 0.25f);
    const DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f);
    const DirectX::XMVECTOR target = DirectX::XMVectorZero();
    const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, target, up);
    const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60.0f), aspectRatio, 0.1f, 100.0f);

	Renderer.UpdateConstant(world * view * projection);
	Renderer.PrepareShader();
	Renderer.RenderPrimitive(VertexBuffer, VertexCount);
}
