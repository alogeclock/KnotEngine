#include "Render/D3D11/D3D11RenderDevice.h"

#include "Core/Assert.h"
#include "Render/D3DCommon.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <limits>

#pragma comment(lib, "d3dcompiler.lib")

FD3D11RenderDevice::FD3D11RenderDevice() = default;

// 소멸 시 Device와 Device가 소유한 모든 GPU 자원을 해제한다.
FD3D11RenderDevice::~FD3D11RenderDevice()
{
	Release();
}

// Native D3D11 Device와 Immediate Context를 생성해 자원 생성 및 명령 실행이 가능한 상태로 만든다.
void FD3D11RenderDevice::Create()
{
	// 재초기화 시 이전 Device와 그에 종속된 모든 자원을 먼저 정리한다.
	Release();
	NativeDevice.Create();
}

// 모든 RHI Handle을 무효화하고 GPU 자원, Context, Device 순서로 해제한다.
void FD3D11RenderDevice::Release()
{
	// 남아 있는 논리 Command List Handle을 무효화해 재생성된 Device에서 재사용하지 못하게 한다.
	bCommandListOpen = false;
	AdvanceGeneration(CommandListGeneration);

	PipelineSlots.clear();
	ShaderSlots.clear();
	TextureSlots.clear();
	BufferPool.Release();
	NativeDevice.FlushAndUnbindTargets();
	NativeDevice.Release();
}

// 공통 Buffer Description을 D3D11 Buffer Pool에 전달하고 유효한 Handle을 반환한다.
FBufferHandle FD3D11RenderDevice::CreateBuffer(const FBufferDesc& Desc, std::span<const uint8> InitialData)
{
	return BufferPool.CreateBuffer(NativeDevice.GetDevice(), Desc, InitialData);
}

// CPU 쓰기가 허용된 Buffer 전체를 새 데이터로 갱신한다.
void FD3D11RenderDevice::UpdateBuffer(FBufferHandle Handle, std::span<const uint8> Data)
{
	BufferPool.UpdateBuffer(NativeDevice.GetContext(), Handle, Data);
}

// Buffer Handle이 가리키는 네이티브 자원을 해제하고 Handle을 무효화한다.
void FD3D11RenderDevice::DestroyBuffer(FBufferHandle& Handle)
{
	BufferPool.DestroyBuffer(Handle);
}

// 공통 Texture Description과 초기 데이터를 D3D11 2D Texture로 변환해 생성한다.
FTextureHandle FD3D11RenderDevice::CreateTexture(
	const FTextureDesc& Desc, std::span<const uint8> InitialData)
{
	panic(NativeDevice.GetDevice());
	panicf(Desc.Width > 0 && Desc.Height > 0, "잘못된 Texture 크기. Width={}, Height={}", Desc.Width, Desc.Height);

	// RHI Description을 D3D11 Texture Description으로 변환한다.
	D3D11_TEXTURE2D_DESC NativeDesc = {};
	NativeDesc.Width = Desc.Width;
	NativeDesc.Height = Desc.Height;
	NativeDesc.MipLevels = 1;
	NativeDesc.ArraySize = 1;
	NativeDesc.SampleDesc.Count = 1;
	switch (Desc.Format)
	{
	case ETextureFormat::RGBA8UNorm: NativeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
	case ETextureFormat::BGRA8UNorm: NativeDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
	case ETextureFormat::D24UNormS8UInt: NativeDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; break;
	}
	switch (Desc.Usage)
	{
	case ETextureUsage::ShaderResource: NativeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; break;
	case ETextureUsage::RenderTarget: NativeDesc.BindFlags = D3D11_BIND_RENDER_TARGET; break;
	case ETextureUsage::DepthStencil: NativeDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL; break;
	}

	D3D11_SUBRESOURCE_DATA NativeInitialData = {};
	if (!InitialData.empty())
	{
		panicf(Desc.Format != ETextureFormat::D24UNormS8UInt && InitialData.size() == static_cast<size_t>(Desc.Width) * Desc.Height * 4,
			"Texture 초기 데이터 크기 불일치. Bytes={}", InitialData.size());
		NativeInitialData.pSysMem = InitialData.data();
		NativeInitialData.SysMemPitch = Desc.Width * 4;
	}

	// 네이티브 생성 결과까지 검증한 다음에만 외부에서 사용할 Handle 슬롯에 보관한다.
	FTextureSlot Slot;
	const HRESULT Result = NativeDevice.GetDevice()->CreateTexture2D(
		&NativeDesc, InitialData.empty() ? nullptr : &NativeInitialData, Slot.Texture.GetAddressOf());
	panicf(SUCCEEDED(Result) && Slot.Texture, "ID3D11Device::CreateTexture2D 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	panicf(TextureSlots.size() < (std::numeric_limits<uint32>::max)(), "D3D11 Texture 슬롯 수가 uint32 범위를 초과했다.");
	TextureSlots.push_back(std::move(Slot));
	return { static_cast<uint32>(TextureSlots.size() - 1), TextureSlots.back().Generation };
}

// Texture 슬롯의 자원을 해제하고 Generation을 증가시켜 과거 Handle의 접근을 차단한다.
void FD3D11RenderDevice::DestroyTexture(FTextureHandle& Handle)
{
	checkf(!Handle.IsValid() || Handle.Index < TextureSlots.size(), "유효하지 않은 Texture 핸들. Index={}", Handle.Index);
	if (Handle.IsValid() && Handle.Index < TextureSlots.size())
	{
		FTextureSlot& Slot = TextureSlots[Handle.Index];
		if (Slot.Generation == Handle.Generation)
		{
			Slot.Texture.Reset();
			AdvanceGeneration(Slot.Generation);
		}
	}
	Handle.Reset();
}

// HLSL Source를 Stage별 Shader Model로 컴파일하고 네이티브 Shader 객체와 Bytecode를 함께 보관한다.
FShaderHandle FD3D11RenderDevice::CreateShader(const FShaderDesc& Desc)
{
	panic(NativeDevice.GetDevice());
	panicf(!Desc.SourcePath.empty() && !Desc.EntryPoint.empty(), "Shader 생성 정보가 비어 있다.");

	// RHI는 Shader Model을 노출하지 않고 각 백엔드가 지원하는 Target을 선택한다.
	UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(KNOT_BUILD_DEBUG)
	CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	CompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	FShaderSlot Slot;
	Slot.Stage = Desc.Stage;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	const char* Target = Desc.Stage == EShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
	HRESULT Result = D3DCompileFromFile(
		Desc.SourcePath.c_str(), nullptr, nullptr, Desc.EntryPoint.c_str(), Target, CompileFlags, 0,
		Slot.Bytecode.GetAddressOf(), ErrorBlob.GetAddressOf());
	panicf(SUCCEEDED(Result) && Slot.Bytecode, "Shader 컴파일 실패. HRESULT=0x{:08X}\n{}",
		static_cast<uint32>(Result), GetShaderError(ErrorBlob.Get()));

	if (Desc.Stage == EShaderStage::Vertex)
	{
		Result = NativeDevice.GetDevice()->CreateVertexShader(
			Slot.Bytecode->GetBufferPointer(), Slot.Bytecode->GetBufferSize(), nullptr, Slot.VertexShader.GetAddressOf());
	}
	else
	{
		Result = NativeDevice.GetDevice()->CreatePixelShader(
			Slot.Bytecode->GetBufferPointer(), Slot.Bytecode->GetBufferSize(), nullptr, Slot.PixelShader.GetAddressOf());
	}
	panicf(SUCCEEDED(Result), "D3D11 Shader 생성 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	panicf(ShaderSlots.size() < (std::numeric_limits<uint32>::max)(), "D3D11 Shader 슬롯 수가 uint32 범위를 초과했다.");
	ShaderSlots.push_back(std::move(Slot));
	return { static_cast<uint32>(ShaderSlots.size() - 1), ShaderSlots.back().Generation };
}

// Shader 객체와 Input Layout 생성에 사용한 Bytecode를 함께 해제한다.
void FD3D11RenderDevice::DestroyShader(FShaderHandle& Handle)
{
	FShaderSlot* Slot = ResolveShader(Handle);
	if (Slot)
	{
		Slot->VertexShader.Reset();
		Slot->PixelShader.Reset();
		Slot->Bytecode.Reset();
		AdvanceGeneration(Slot->Generation);
	}
	Handle.Reset();
}

// Shader, Vertex Layout, Depth 및 Rasterizer 상태를 하나의 Graphics Pipeline Handle로 묶는다.
FGraphicsPipelineHandle FD3D11RenderDevice::CreateGraphicsPipeline(const FGraphicsPipelineDesc& Desc)
{
	FShaderSlot* VertexShader = ResolveShader(Desc.VertexShader);
	FShaderSlot* PixelShader = ResolveShader(Desc.PixelShader);
	panicf(VertexShader && VertexShader->Stage == EShaderStage::Vertex && VertexShader->VertexShader, "Graphics Pipeline에 유효한 Vertex Shader가 필요하다.");
	panicf(PixelShader && PixelShader->Stage == EShaderStage::Pixel && PixelShader->PixelShader, "Graphics Pipeline에 유효한 Pixel Shader가 필요하다.");
	panicf(!Desc.VertexLayout.Elements.empty() && Desc.VertexLayout.Stride > 0, "Graphics Pipeline에 유효한 Vertex Layout이 필요하다.");

	FPipelineSlot Slot;
	Slot.VertexShader = Desc.VertexShader;
	Slot.PixelShader = Desc.PixelShader;
	Slot.PrimitiveTopology = Desc.PrimitiveTopology;

	// API 중립 Vertex Layout을 Vertex Shader 입력 Signature와 결합한다.
	TArray<D3D11_INPUT_ELEMENT_DESC> LayoutDescs;
	LayoutDescs.reserve(Desc.VertexLayout.Elements.size());
	for (const FVertexElement& Element : Desc.VertexLayout.Elements)
	{
		const char* SemanticName = GetSemanticName(Element.Semantic);
		const DXGI_FORMAT Format = GetDXGIFormat(Element.Format);
		panicf(SemanticName && Format != DXGI_FORMAT_UNKNOWN, "지원하지 않는 Vertex Element.");

		D3D11_INPUT_ELEMENT_DESC NativeElement = {};
		NativeElement.SemanticName = SemanticName;
		NativeElement.SemanticIndex = Element.SemanticIndex;
		NativeElement.Format = Format;
		NativeElement.AlignedByteOffset = Element.Offset;
		NativeElement.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		LayoutDescs.push_back(NativeElement);
	}

	HRESULT Result = NativeDevice.GetDevice()->CreateInputLayout(
		LayoutDescs.data(), static_cast<UINT>(LayoutDescs.size()),
		VertexShader->Bytecode->GetBufferPointer(), VertexShader->Bytecode->GetBufferSize(), Slot.InputLayout.GetAddressOf());
	panicf(SUCCEEDED(Result) && Slot.InputLayout, "ID3D11Device::CreateInputLayout 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_DEPTH_STENCIL_DESC DepthDesc = {};
	DepthDesc.DepthEnable = Desc.bDepthTestEnabled;
	DepthDesc.DepthWriteMask = Desc.bDepthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthDesc.DepthFunc = D3D11_COMPARISON_LESS;
	Result = NativeDevice.GetDevice()->CreateDepthStencilState(&DepthDesc, Slot.DepthStencilState.GetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateDepthStencilState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;
	Result = NativeDevice.GetDevice()->CreateRasterizerState(&RasterizerDesc, Slot.RasterizerState.GetAddressOf());
	panicf(SUCCEEDED(Result), "ID3D11Device::CreateRasterizerState 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));

	Slot.bValid = true;
	panicf(PipelineSlots.size() < (std::numeric_limits<uint32>::max)(), "D3D11 Pipeline 슬롯 수가 uint32 범위를 초과했다.");
	PipelineSlots.push_back(std::move(Slot));
	return { static_cast<uint32>(PipelineSlots.size() - 1), PipelineSlots.back().Generation };
}

// Graphics Pipeline이 소유한 Input Layout과 고정 기능 상태 객체를 해제한다.
void FD3D11RenderDevice::DestroyGraphicsPipeline(FGraphicsPipelineHandle& Handle)
{
	FPipelineSlot* Slot = ResolvePipeline(Handle);
	if (Slot)
	{
		Slot->DepthStencilState.Reset();
		Slot->RasterizerState.Reset();
		Slot->InputLayout.Reset();
		Slot->bValid = false;
		AdvanceGeneration(Slot->Generation);
	}
	Handle.Reset();
}

// Immediate Context 명령을 기록할 논리 Command List 구간을 시작한다.
FCommandListHandle FD3D11RenderDevice::BeginCommandList()
{
	// D3D11 명령은 Immediate Context에서 즉시 실행되지만 상위 계층에는 명시적 기록 구간을 제공한다.
	panic(NativeDevice.GetContext());
	checkf(!bCommandListOpen, "D3D11 Command List가 이미 열려 있다.");
	bCommandListOpen = true;
	return { 0, CommandListGeneration };
}

// 열린 논리 Command List를 닫아 Submit 가능한 상태로 전환한다.
void FD3D11RenderDevice::EndCommandList(FCommandListHandle CommandList)
{
	ValidateCommandList(CommandList);
	bCommandListOpen = false;
}

// 종료된 논리 Command List의 사용을 완료하고 Handle을 무효화한다.
void FD3D11RenderDevice::Submit(FCommandListHandle& CommandList)
{
	// Immediate Context에는 별도 제출이 없으므로 순서만 검증하고 논리 Handle을 소모한다.
	checkf(CommandList.IsValid() && CommandList.Index == 0 && CommandList.Generation == CommandListGeneration && !bCommandListOpen,
		"종료되지 않았거나 유효하지 않은 D3D11 Command List 제출.");
	CommandList.Reset();
	AdvanceGeneration(CommandListGeneration);
}

// Pipeline Handle에 묶인 Shader, Input Layout 및 고정 기능 상태를 Immediate Context에 설정한다.
void FD3D11RenderDevice::SetGraphicsPipeline(FCommandListHandle CommandList, FGraphicsPipelineHandle Pipeline)
{
	ValidateCommandList(CommandList);
	FPipelineSlot* PipelineSlot = ResolvePipeline(Pipeline);
	panic(PipelineSlot);
	FShaderSlot* VertexShader = ResolveShader(PipelineSlot->VertexShader);
	FShaderSlot* PixelShader = ResolveShader(PipelineSlot->PixelShader);
	panic(VertexShader && PixelShader);

	// ImGui 등 외부 렌더러가 Immediate Context 상태를 변경할 수 있으므로 프레임마다 전체 상태를 다시 설정한다.
	ID3D11DeviceContext* Context = NativeDevice.GetContext();
	Context->IASetInputLayout(PipelineSlot->InputLayout.Get());
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->RSSetState(PipelineSlot->RasterizerState.Get());
	Context->OMSetDepthStencilState(PipelineSlot->DepthStencilState.Get(), 0);
	Context->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	Context->VSSetShader(VertexShader->VertexShader.Get(), nullptr, 0);
	Context->PSSetShader(PixelShader->PixelShader.Get(), nullptr, 0);
}

// Vertex Buffer와 Vertex 단위 Stride 및 시작 Offset을 Input Assembler에 설정한다.
void FD3D11RenderDevice::SetVertexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, uint32 Stride, uint32 Offset)
{
	ValidateCommandList(CommandList);
	ID3D11Buffer* NativeBuffer = BufferPool.ResolveBuffer(Buffer);
	const FBufferDesc* Desc = BufferPool.ResolveDesc(Buffer);
	panicf(NativeBuffer && Desc && Desc->Usage == EBufferUsage::Vertex && Stride > 0, "유효하지 않은 Vertex Buffer 바인딩.");
	NativeDevice.GetContext()->IASetVertexBuffers(0, 1, &NativeBuffer, &Stride, &Offset);
}

// Index Buffer와 Index Format 및 시작 Offset을 Input Assembler에 설정한다.
void FD3D11RenderDevice::SetIndexBuffer(FCommandListHandle CommandList, FBufferHandle Buffer, EIndexFormat Format, uint32 Offset)
{
	ValidateCommandList(CommandList);
	ID3D11Buffer* NativeBuffer = BufferPool.ResolveBuffer(Buffer);
	const FBufferDesc* Desc = BufferPool.ResolveDesc(Buffer);
	panicf(NativeBuffer && Desc && Desc->Usage == EBufferUsage::Index, "유효하지 않은 Index Buffer 바인딩.");
	const DXGI_FORMAT NativeFormat = Format == EIndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	NativeDevice.GetContext()->IASetIndexBuffer(NativeBuffer, NativeFormat, Offset);
}

// Constant Buffer를 지정한 Shader Stage와 Register Slot에 설정한다.
void FD3D11RenderDevice::SetConstantBuffer(FCommandListHandle CommandList, EShaderStage Stage, uint32 Slot, FBufferHandle Buffer)
{
	ValidateCommandList(CommandList);
	ID3D11Buffer* NativeBuffer = BufferPool.ResolveBuffer(Buffer);
	const FBufferDesc* Desc = BufferPool.ResolveDesc(Buffer);
	panicf(NativeBuffer && Desc && Desc->Usage == EBufferUsage::Constant, "유효하지 않은 Constant Buffer 바인딩.");
	if (Stage == EShaderStage::Vertex)
	{
		NativeDevice.GetContext()->VSSetConstantBuffers(Slot, 1, &NativeBuffer);
	}
	else
	{
		NativeDevice.GetContext()->PSSetConstantBuffers(Slot, 1, &NativeBuffer);
	}
}

// 현재 Graphics 상태를 사용해 비인덱스 Geometry를 그린다.
void FD3D11RenderDevice::Draw(FCommandListHandle CommandList, uint32 VertexCount, uint32 FirstVertex)
{
	ValidateCommandList(CommandList);
	panic(VertexCount > 0);
	NativeDevice.GetContext()->Draw(VertexCount, FirstVertex);
}

// 현재 Graphics 상태와 Index Buffer를 사용해 인덱스 Geometry를 그린다.
void FD3D11RenderDevice::DrawIndexed(FCommandListHandle CommandList, uint32 IndexCount, uint32 FirstIndex, int32 VertexOffset)
{
	ValidateCommandList(CommandList);
	panic(IndexCount > 0);
	NativeDevice.GetContext()->DrawIndexed(IndexCount, FirstIndex, VertexOffset);
}

// Command List가 현재 Device에서 열린 최신 기록 구간을 가리키는지 검증한다.
void FD3D11RenderDevice::ValidateCommandList(FCommandListHandle CommandList) const
{
	checkf(bCommandListOpen && CommandList.IsValid() && CommandList.Index == 0 && CommandList.Generation == CommandListGeneration,
		"유효하지 않은 D3D11 Command List. Index={}, Generation={}", CommandList.Index, CommandList.Generation);
}

// 수정 가능한 Shader 슬롯 조회를 const 조회 구현과 동일한 검증 경로로 처리한다.
FD3D11RenderDevice::FShaderSlot* FD3D11RenderDevice::ResolveShader(FShaderHandle Handle)
{
	return const_cast<FShaderSlot*>(static_cast<const FD3D11RenderDevice*>(this)->ResolveShader(Handle));
}

// Index와 Generation이 모두 일치하는 Shader 슬롯만 반환한다.
const FD3D11RenderDevice::FShaderSlot* FD3D11RenderDevice::ResolveShader(FShaderHandle Handle) const
{
	// 파괴된 슬롯과 동일한 Index를 가진 과거 Handle은 Generation 비교에서 거부한다.
	if (!Handle.IsValid() || Handle.Index >= ShaderSlots.size())
	{
		return nullptr;
	}
	const FShaderSlot& Slot = ShaderSlots[Handle.Index];
	return Slot.Generation == Handle.Generation && Slot.Bytecode ? &Slot : nullptr;
}

// Index와 Generation이 일치하고 생성이 완료된 Pipeline 슬롯만 반환한다.
FD3D11RenderDevice::FPipelineSlot* FD3D11RenderDevice::ResolvePipeline(FGraphicsPipelineHandle Handle)
{
	if (!Handle.IsValid() || Handle.Index >= PipelineSlots.size())
	{
		return nullptr;
	}
	FPipelineSlot& Slot = PipelineSlots[Handle.Index];
	return Slot.Generation == Handle.Generation && Slot.bValid ? &Slot : nullptr;
}

// 파괴된 슬롯의 과거 Handle이 다시 유효해지지 않도록 Generation을 다음 유효 값으로 전진시킨다.
void FD3D11RenderDevice::AdvanceGeneration(uint32& Generation)
{
	// 0은 초기화되지 않은 Generation으로 남겨 두기 위해 Overflow 시 1로 순환한다.
	++Generation;
	if (Generation == 0)
	{
		Generation = 1;
	}
}
