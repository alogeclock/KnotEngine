#pragma once

#include "Render/Resource/VertexLayouts.h"

#include <wrl/client.h>

struct FMatrix;
struct ID3D11Buffer;
struct ID3D11DepthStencilState;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11InputLayout;
struct ID3D11PixelShader;
struct ID3D11RasterizerState;
struct ID3D11VertexShader;
struct ID3D10Blob;

class FD3D11Pipeline final
{
public:
	void Create(ID3D11Device* Device);
	void Release();

	void PrepareFrame(ID3D11DeviceContext* DeviceContext);
	void PrepareShader(ID3D11DeviceContext* DeviceContext);
	void UpdateConstant(ID3D11DeviceContext* DeviceContext, const FMatrix& WorldViewProjection);
	ID3D11InputLayout* GetOrCreateInputLayout(ID3D11Device* Device, const FVertexLayout& VertexLayout);

private:
	struct FConstants
	{
		alignas(16) float ModelViewProjection[4][4];
	};

	// ComPtr<T>: COM 객체의 수명을 자동으로 관리하는 스마트 포인터
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
	Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
	Microsoft::WRL::ComPtr<ID3D10Blob> VertexShaderInputSignature;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
	FVertexLayout InputLayoutDescription;
};
