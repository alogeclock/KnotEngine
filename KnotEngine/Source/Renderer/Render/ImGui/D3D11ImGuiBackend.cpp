#include "Render/ImGui/D3D11ImGuiBackend.h"

#include "Core/Assert.h"
#include "Render/D3D11/D3D11RenderDevice.h"
#include "Render/ImGui/ImGuiDrawDataCopy.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>
#include <limits>

struct FD3D11ImGuiVertexConstants
{
	float Projection[4][4];
};

FD3D11ImGuiBackend::FD3D11ImGuiBackend(FD3D11RenderDevice& InRenderDevice)
	: RenderDevice(InRenderDevice)
{
}

FD3D11ImGuiBackend::~FD3D11ImGuiBackend() = default;

ImTextureID FD3D11ImGuiBackend::Startup(std::span<const uint8> FontPixels, uint32 FontWidth, uint32 FontHeight)
{
	check(!FontTexture.IsValid() && !FontPixels.empty() && FontWidth > 0 && FontHeight > 0);
	ID3D11Device* Device = RenderDevice.GetNativeDevice();
	check(Device);
	static constexpr const char* VertexShaderSource = R"(
		cbuffer vertexBuffer : register(b0) { float4x4 ProjectionMatrix; };
		struct VS_INPUT { float2 Position : POSITION; float4 Color : COLOR0; float2 UV : TEXCOORD0; };
		struct PS_INPUT { float4 Position : SV_POSITION; float4 Color : COLOR0; float2 UV : TEXCOORD0; };
		PS_INPUT main(VS_INPUT Input)
		{
			PS_INPUT Output;
			Output.Position = mul(ProjectionMatrix, float4(Input.Position, 0.0f, 1.0f));
			Output.Color = Input.Color;
			Output.UV = Input.UV;
			return Output;
		})";
	static constexpr const char* PixelShaderSource = R"(
		sampler Sampler : register(s0);
		Texture2D Texture : register(t0);
		struct PS_INPUT { float4 Position : SV_POSITION; float4 Color : COLOR0; float2 UV : TEXCOORD0; };
		float4 main(PS_INPUT Input) : SV_Target { return Input.Color * Texture.Sample(Sampler, Input.UV); })";

	Microsoft::WRL::ComPtr<ID3DBlob> VertexShaderBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> PixelShaderBlob;
	panicf(SUCCEEDED(D3DCompile(VertexShaderSource, std::strlen(VertexShaderSource), nullptr, nullptr, nullptr, "main", "vs_4_0", 0, 0,
		&VertexShaderBlob, nullptr)), "ImGui Vertex Shader 컴파일 실패.");
	panicf(SUCCEEDED(D3DCompile(PixelShaderSource, std::strlen(PixelShaderSource), nullptr, nullptr, nullptr, "main", "ps_4_0", 0, 0,
		&PixelShaderBlob, nullptr)), "ImGui Pixel Shader 컴파일 실패.");
	panicf(SUCCEEDED(Device->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, &VertexShader)),
		"ImGui Vertex Shader 생성 실패.");
	panicf(SUCCEEDED(Device->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, &PixelShader)),
		"ImGui Pixel Shader 생성 실패.");

	const D3D11_INPUT_ELEMENT_DESC InputElements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImDrawVert, pos)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(ImDrawVert, uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(ImDrawVert, col)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	panicf(SUCCEEDED(Device->CreateInputLayout(InputElements, static_cast<UINT>(std::size(InputElements)), VertexShaderBlob->GetBufferPointer(),
		VertexShaderBlob->GetBufferSize(), &InputLayout)), "ImGui Input Layout 생성 실패.");

	D3D11_BUFFER_DESC ConstantBufferDesc = {};
	ConstantBufferDesc.ByteWidth = sizeof(FD3D11ImGuiVertexConstants);
	ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	panicf(SUCCEEDED(Device->CreateBuffer(&ConstantBufferDesc, nullptr, &VertexConstantBuffer)), "ImGui Vertex Constant Buffer 생성 실패.");

	D3D11_BLEND_DESC BlendDesc = {};
	BlendDesc.RenderTarget[0].BlendEnable = true;
	BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	panicf(SUCCEEDED(Device->CreateBlendState(&BlendDesc, &BlendState)), "ImGui Blend State 생성 실패.");

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.ScissorEnable = true;
	RasterizerDesc.DepthClipEnable = true;
	panicf(SUCCEEDED(Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState)), "ImGui Rasterizer State 생성 실패.");

	D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
	DepthDesc.DepthEnable = false;
	DepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	panicf(SUCCEEDED(Device->CreateDepthStencilState(&DepthDesc, &DepthStencilState)), "ImGui Depth Stencil State 생성 실패.");

	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	panicf(SUCCEEDED(Device->CreateSamplerState(&SamplerDesc, &TextureSampler)), "ImGui Texture Sampler 생성 실패.");

	FTextureDesc FontDesc;
	FontDesc.Width = FontWidth;
	FontDesc.Height = FontHeight;
	FontDesc.Format = ETextureFormat::RGBA8UNorm;
	FontDesc.Usage = ETextureUsage::ShaderResource;
	const FTextureSubresourceData FontData = { FontPixels, FontWidth * 4, static_cast<uint32>(FontPixels.size()) };
	FontTexture = RenderDevice.CreateTexture(FontDesc, std::span(&FontData, 1));
	panic(FontTexture.IsValid());
	return GetImGuiTextureID(FontTexture);
}

bool FD3D11ImGuiBackend::EnsureBuffers(SIZE_T VertexCount, SIZE_T IndexCount)
{
	ID3D11Device* Device = RenderDevice.GetNativeDevice();
	check(Device);
	if (!VertexBuffer || VertexBufferSize < VertexCount)
	{
		VertexBuffer.Reset();
		VertexBufferSize = VertexCount + 5000;
		panicf(VertexBufferSize <= (std::numeric_limits<UINT>::max)() / sizeof(ImDrawVert), "ImGui Vertex Buffer 크기가 D3D11 범위를 초과했다.");
		D3D11_BUFFER_DESC Desc = {};
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.ByteWidth = static_cast<UINT>(VertexBufferSize * sizeof(ImDrawVert));
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(Device->CreateBuffer(&Desc, nullptr, &VertexBuffer)))
		{
			return false;
		}
	}
	if (!IndexBuffer || IndexBufferSize < IndexCount)
	{
		IndexBuffer.Reset();
		IndexBufferSize = IndexCount + 10000;
		panicf(IndexBufferSize <= (std::numeric_limits<UINT>::max)() / sizeof(ImDrawIdx), "ImGui Index Buffer 크기가 D3D11 범위를 초과했다.");
		D3D11_BUFFER_DESC Desc = {};
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.ByteWidth = static_cast<UINT>(IndexBufferSize * sizeof(ImDrawIdx));
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(Device->CreateBuffer(&Desc, nullptr, &IndexBuffer)))
		{
			return false;
		}
	}
	return true;
}

void FD3D11ImGuiBackend::SetupRenderState(const FImGuiDrawDataCopy& DrawData)
{
	ID3D11DeviceContext* Context = RenderDevice.GetNativeContext();
	check(Context);
	D3D11_VIEWPORT Viewport = {};
	Viewport.Width = DrawData.DisplaySize.x * DrawData.FramebufferScale.x;
	Viewport.Height = DrawData.DisplaySize.y * DrawData.FramebufferScale.y;
	Viewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &Viewport);

	D3D11_MAPPED_SUBRESOURCE MappedConstants = {};
	panicf(SUCCEEDED(Context->Map(VertexConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedConstants)), "ImGui 상수 버퍼 Map 실패.");
	auto* Constants = static_cast<FD3D11ImGuiVertexConstants*>(MappedConstants.pData);
	const float Left = DrawData.DisplayPosition.x;
	const float Right = DrawData.DisplayPosition.x + DrawData.DisplaySize.x;
	const float Top = DrawData.DisplayPosition.y;
	const float Bottom = DrawData.DisplayPosition.y + DrawData.DisplaySize.y;
	const float Projection[4][4] = {
		{ 2.0f / (Right - Left), 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f / (Top - Bottom), 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.5f, 0.0f },
		{ (Right + Left) / (Left - Right), (Top + Bottom) / (Bottom - Top), 0.5f, 1.0f },
	};
	std::memcpy(Constants->Projection, Projection, sizeof(Projection));
	Context->Unmap(VertexConstantBuffer.Get(), 0);

	const UINT Stride = sizeof(ImDrawVert);
	const UINT Offset = 0;
	ID3D11Buffer* VertexBufferPointer = VertexBuffer.Get();
	ID3D11Buffer* ConstantBufferPointer = VertexConstantBuffer.Get();
	ID3D11SamplerState* SamplerPointer = TextureSampler.Get();
	Context->IASetInputLayout(InputLayout.Get());
	Context->IASetVertexBuffers(0, 1, &VertexBufferPointer, &Stride, &Offset);
	Context->IASetIndexBuffer(IndexBuffer.Get(), sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->VSSetShader(VertexShader.Get(), nullptr, 0);
	Context->VSSetConstantBuffers(0, 1, &ConstantBufferPointer);
	Context->PSSetShader(PixelShader.Get(), nullptr, 0);
	Context->PSSetSamplers(0, 1, &SamplerPointer);
	Context->GSSetShader(nullptr, nullptr, 0);
	Context->HSSetShader(nullptr, nullptr, 0);
	Context->DSSetShader(nullptr, nullptr, 0);
	Context->CSSetShader(nullptr, nullptr, 0);
	const float BlendFactor[4] = {};
	Context->OMSetBlendState(BlendState.Get(), BlendFactor, 0xffffffff);
	Context->OMSetDepthStencilState(DepthStencilState.Get(), 0);
	Context->RSSetState(RasterizerState.Get());
}

void FD3D11ImGuiBackend::Render(FCommandListHandle CommandList, const FImGuiDrawDataCopy& DrawData)
{
	check(CommandList.IsValid());
	if (!DrawData.IsVisible())
	{
		return;
	}

	SIZE_T VertexCount = 0;
	SIZE_T IndexCount = 0;
	for (const FImGuiDrawListCopy& DrawList : DrawData.DrawLists)
	{
		VertexCount += DrawList.Vertices.size();
		IndexCount += DrawList.Indices.size();
	}
	panicf(EnsureBuffers(VertexCount, IndexCount), "ImGui Dynamic Buffer 생성 실패.");

	ID3D11DeviceContext* Context = RenderDevice.GetNativeContext();
	D3D11_MAPPED_SUBRESOURCE MappedVertices = {};
	D3D11_MAPPED_SUBRESOURCE MappedIndices = {};
	panicf(SUCCEEDED(Context->Map(VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVertices)), "ImGui Vertex Buffer Map 실패.");
	if (FAILED(Context->Map(IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedIndices)))
	{
		Context->Unmap(VertexBuffer.Get(), 0);
		panicf(false, "ImGui Index Buffer Map 실패.");
	}
	auto* VertexDestination = static_cast<ImDrawVert*>(MappedVertices.pData);
	auto* IndexDestination = static_cast<ImDrawIdx*>(MappedIndices.pData);
	for (const FImGuiDrawListCopy& DrawList : DrawData.DrawLists)
	{
		std::memcpy(VertexDestination, DrawList.Vertices.data(), DrawList.Vertices.size() * sizeof(ImDrawVert));
		std::memcpy(IndexDestination, DrawList.Indices.data(), DrawList.Indices.size() * sizeof(ImDrawIdx));
		VertexDestination += DrawList.Vertices.size();
		IndexDestination += DrawList.Indices.size();
	}
	Context->Unmap(VertexBuffer.Get(), 0);
	Context->Unmap(IndexBuffer.Get(), 0);

	SetupRenderState(DrawData);
	int32 GlobalIndexOffset = 0;
	int32 GlobalVertexOffset = 0;
	for (const FImGuiDrawListCopy& DrawList : DrawData.DrawLists)
	{
		for (const FImGuiDrawCommand& Command : DrawList.Commands)
		{
			if (Command.bResetRenderState)
			{
				SetupRenderState(DrawData);
				continue;
			}
			const ImVec2 ClipMinimum((Command.ClipRect.x - DrawData.DisplayPosition.x) * DrawData.FramebufferScale.x,
			                         (Command.ClipRect.y - DrawData.DisplayPosition.y) * DrawData.FramebufferScale.y);
			const ImVec2 ClipMaximum((Command.ClipRect.z - DrawData.DisplayPosition.x) * DrawData.FramebufferScale.x,
			                         (Command.ClipRect.w - DrawData.DisplayPosition.y) * DrawData.FramebufferScale.y);
			if (ClipMaximum.x <= ClipMinimum.x || ClipMaximum.y <= ClipMinimum.y)
			{
				continue;
			}
			const D3D11_RECT ScissorRect = { static_cast<LONG>(ClipMinimum.x), static_cast<LONG>(ClipMinimum.y),
			                                 static_cast<LONG>(ClipMaximum.x), static_cast<LONG>(ClipMaximum.y) };
			Context->RSSetScissorRects(1, &ScissorRect);
			auto* Texture = reinterpret_cast<ID3D11ShaderResourceView*>(Command.TextureId);
			Context->PSSetShaderResources(0, 1, &Texture);
			Context->DrawIndexed(Command.ElementCount, Command.IndexOffset + GlobalIndexOffset, Command.VertexOffset + GlobalVertexOffset);
		}
		GlobalIndexOffset += static_cast<int32>(DrawList.Indices.size());
		GlobalVertexOffset += static_cast<int32>(DrawList.Vertices.size());
	}
}

ImTextureID FD3D11ImGuiBackend::GetImGuiTextureID(FTextureHandle Texture) const
{
	return reinterpret_cast<ImTextureID>(RenderDevice.GetNativeShaderResourceView(Texture));
}

void FD3D11ImGuiBackend::Shutdown()
{
	if (FontTexture.IsValid())
	{
		RenderDevice.DestroyTexture(FontTexture);
	}
	DepthStencilState.Reset();
	BlendState.Reset();
	RasterizerState.Reset();
	TextureSampler.Reset();
	InputLayout.Reset();
	PixelShader.Reset();
	VertexShader.Reset();
	VertexConstantBuffer.Reset();
	IndexBuffer.Reset();
	VertexBuffer.Reset();
	VertexBufferSize = 0;
	IndexBufferSize = 0;
}
