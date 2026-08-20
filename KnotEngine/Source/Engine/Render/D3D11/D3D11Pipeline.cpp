#include "Render/D3D11/D3D11Pipeline.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/Math/Matrix.h"
#include "Render/D3DCommon.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstring>
#include <utility>

#pragma comment(lib, "d3dcompiler.lib")

// Depth State, Rasterizer, Shader 및 Constant Buffer 등 파이프라인 상태를 정의하여 생성한다.
void FD3D11Pipeline::Create(ID3D11Device* Device)
{
	Release();
	panic(Device);

	// Depth Test 상태 객체 생성: 깊이 테스트 활성화 및 Depth Test를 통과한 픽셀의 Depth를 Depth Buffer에 기록
	D3D11_DEPTH_STENCIL_DESC DepthStateDesc = {};
	DepthStateDesc.DepthEnable = TRUE;
	DepthStateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStateDesc.DepthFunc = D3D11_COMPARISON_LESS; // 새 픽셀의 깊이가 기존 Depth Buffer보다 작을 때만 깊이 테스트 통과
	HRESULT Result = Device->CreateDepthStencilState(&DepthStateDesc, DepthStencilState.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;
	Result = Device->CreateRasterizerState(&RasterizerDesc, RasterizerState.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRasterizerState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderCode;
	Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderCode;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;

	const FWString ShaderPath = FPaths::ShaderDir() + L"Common.hlsl";
	UINT ShaderCompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;

	// Shader Compile Flags: 디버그 정보를 포함하여 셰이더를 컴파일할지, 최적화 과정을 생략할지 선택한다.
#if defined(KNOT_BUILD_DEBUG)
	ShaderCompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	ShaderCompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	Result = D3DCompileFromFile(
	    ShaderPath.c_str(), nullptr, nullptr, "VS", "vs_5_0", ShaderCompileFlags, 0,
	    VertexShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "Vertex Shader 컴파일 실패. HRESULT=0x{:08X}\n{}", static_cast<uint32>(Result), GetShaderError(ErrorBlob.Get()));

	Result = Device->CreateVertexShader(
	    VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(), nullptr,
	    VertexShader.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateVertexShader 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Result = D3DGetInputSignatureBlob(
	    VertexShaderCode->GetBufferPointer(), VertexShaderCode->GetBufferSize(),
	    VertexShaderInputSignature.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "D3DGetInputSignatureBlob 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	ErrorBlob.Reset();
	Result = D3DCompileFromFile(
	    ShaderPath.c_str(), nullptr, nullptr, "PS", "ps_5_0", ShaderCompileFlags, 0,
	    PixelShaderCode.ReleaseAndGetAddressOf(), ErrorBlob.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "Pixel Shader 컴파일 실패. HRESULT=0x{:08X}\n{}", static_cast<uint32>(Result), GetShaderError(ErrorBlob.Get()));

	Result = Device->CreatePixelShader(
	    PixelShaderCode->GetBufferPointer(), PixelShaderCode->GetBufferSize(), nullptr,
	    PixelShader.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreatePixelShader 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	// 상수 버퍼 바인딩
	D3D11_BUFFER_DESC ConstantDesc = {};
	ConstantDesc.ByteWidth = sizeof(FConstants);
	ConstantDesc.Usage = D3D11_USAGE_DYNAMIC; // CPU에서 자주 갱신하는 동적 자원
	ConstantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Result = Device->CreateBuffer(&ConstantDesc, nullptr, ConstantBuffer.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateBuffer(Constant) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
}

void FD3D11Pipeline::Release()
{
	InputLayouts.clear();
	VertexShaderInputSignature.Reset();
	VertexShader.Reset();
	PixelShader.Reset();
	ConstantBuffer.Reset();
	RasterizerState.Reset();
	DepthStencilState.Reset();
}

void FD3D11Pipeline::PrepareFrame(ID3D11DeviceContext* DeviceContext)
{
	panic(DeviceContext);
	panic(RasterizerState);
	panic(DepthStencilState);
	panic(VertexShader);
	panic(PixelShader);
	panic(ConstantBuffer);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->RSSetState(RasterizerState.Get());
	DeviceContext->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	DeviceContext->VSSetShader(VertexShader.Get(), nullptr, 0);
	DeviceContext->PSSetShader(PixelShader.Get(), nullptr, 0);

	ID3D11Buffer* NativeConstantBuffer = ConstantBuffer.Get();
	DeviceContext->VSSetConstantBuffers(0, 1, &NativeConstantBuffer);
}

void FD3D11Pipeline::UpdateConstant(ID3D11DeviceContext* DeviceContext, const FMatrix& WorldViewProjection)
{
	panic(DeviceContext);
	panic(ConstantBuffer);

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	const HRESULT Result = DeviceContext->Map(ConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	if (FAILED(Result))
	{
		KE_LOG_ONCE(LogD3D11, Error, "ID3D11DeviceContext::Map(ConstantBuffer) 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
		return;
	}
	
	std::memcpy(Mapped.pData, WorldViewProjection.M, sizeof(FConstants));
	DeviceContext->Unmap(ConstantBuffer.Get(), 0);
}

ID3D11InputLayout* FD3D11Pipeline::GetOrCreateInputLayout(ID3D11Device* Device, const FVertexLayout& VertexLayout)
{
	for (const FInputLayoutEntry& Entry : InputLayouts)
	{
		if (Entry.Description == VertexLayout)
		{
			return Entry.InputLayout.Get();
		}
	}

	panic(Device);
	panic(VertexShaderInputSignature);
	panicf(!VertexLayout.Elements.empty() && VertexLayout.Elements.size() <= D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT,
	       "FVertexLayout의 Element 개수가 유효하지 않다. Count={}, Max={}",
	       VertexLayout.Elements.size(), D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT);

	// Vertex Layout에서 제공하는 Vertex Element의 Data Layout에 따라 Input Layout 구성
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDescs;
	LayoutDescs.reserve(VertexLayout.Elements.size());
	for (const FVertexElement& Element : VertexLayout.Elements)
	{
		const char* SemanticName = GetSemanticName(Element.Semantic);
		const DXGI_FORMAT Format = GetDXGIFormat(Element.Format);
		panicf(SemanticName, "GetSemanticName()이 처리하지 않는 FVertexSemantic. Value={}", static_cast<uint32>(Element.Semantic));
		panicf(Format != DXGI_FORMAT_UNKNOWN, "GetDXGIFormat()이 처리하지 않는 FVertexFormat. Value={}", static_cast<uint32>(Element.Format));

		D3D11_INPUT_ELEMENT_DESC Desc = {};
		Desc.SemanticName = SemanticName;
		Desc.SemanticIndex = Element.SemanticIndex;
		Desc.Format = Format;
		Desc.AlignedByteOffset = Element.Offset;
		Desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		LayoutDescs.push_back(Desc);
	}
	
	FInputLayoutEntry Entry;
	Entry.Description = VertexLayout;
	const HRESULT Result = Device->CreateInputLayout(
	    LayoutDescs.data(), static_cast<UINT>(LayoutDescs.size()),
	    VertexShaderInputSignature->GetBufferPointer(), VertexShaderInputSignature->GetBufferSize(),
	    Entry.InputLayout.ReleaseAndGetAddressOf());
	panicf(SUCCEEDED(Result) && Entry.InputLayout, "FVertexLayout이 정점 셰이더 입력 시그니처와 불일치. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	InputLayouts.push_back(std::move(Entry));
	return InputLayouts.back().InputLayout.Get();
}
