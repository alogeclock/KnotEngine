#include "Render/D3D11/D3D11ShaderCompiler.h"

#include "Core/Assert.h"
#include "Render/D3DCommon.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include <cstring>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

FCompiledShaderData FD3D11ShaderCompiler::Compile(const FShaderKey& Key, std::span<const uint8> Source)
{
	panicf(!Source.empty() && !Key.SourcePath.empty() && !Key.EntryPoint.empty(), "Shader 컴파일 정보가 비어 있다.");
	UINT CompileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(KNOT_BUILD_DEBUG)
	CompileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	CompileFlags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	// TODO: Key.PermutationId에 대응하는 define 목록을 D3DCompile에 전달한다.
	Microsoft::WRL::ComPtr<ID3DBlob> Bytecode;
	Microsoft::WRL::ComPtr<ID3DBlob> ErrorBlob;
	const char* Target = Key.Stage == EShaderStage::Vertex ? "vs_5_0" : "ps_5_0";
	const HRESULT Result = D3DCompile(Source.data(), Source.size(), Key.SourcePath.c_str(), nullptr, nullptr, Key.EntryPoint.c_str(), Target,
		CompileFlags, 0, Bytecode.GetAddressOf(), ErrorBlob.GetAddressOf());
	panicf(SUCCEEDED(Result) && Bytecode, "Shader 컴파일 실패. Path={}, EntryPoint={}, HRESULT=0x{:08X}\n{}",
		Key.SourcePath, Key.EntryPoint, static_cast<uint32>(Result), GetShaderError(ErrorBlob.Get()));

	FCompiledShaderData Data;
	Data.Bytecode.resize(Bytecode->GetBufferSize());
	std::memcpy(Data.Bytecode.data(), Bytecode->GetBufferPointer(), Bytecode->GetBufferSize());
	Data.Reflection = ReflectShader(*Bytecode.Get(), Key.Stage);
	return Data;
}

FShaderReflection FD3D11ShaderCompiler::ReflectShader(ID3D10Blob& Bytecode, EShaderStage Stage)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> NativeReflection;
	const HRESULT ReflectResult = D3DReflect(Bytecode.GetBufferPointer(), Bytecode.GetBufferSize(), __uuidof(ID3D11ShaderReflection),
		reinterpret_cast<void**>(NativeReflection.GetAddressOf()));
	panicf(SUCCEEDED(ReflectResult) && NativeReflection, "D3DReflect 실패. HRESULT=0x{:08X}", static_cast<uint32>(ReflectResult));

	D3D11_SHADER_DESC ShaderDesc = {};
	panicf(SUCCEEDED(NativeReflection->GetDesc(&ShaderDesc)), "D3D11 Shader Reflection Description을 읽지 못했다.");

	FShaderReflection Reflection;
	Reflection.ConstantBuffers.reserve(ShaderDesc.ConstantBuffers);
	for (uint32 BufferIndex = 0; BufferIndex < ShaderDesc.ConstantBuffers; ++BufferIndex)
	{
		ID3D11ShaderReflectionConstantBuffer* NativeBuffer = NativeReflection->GetConstantBufferByIndex(BufferIndex);
		D3D11_SHADER_BUFFER_DESC BufferDesc = {};
		panicf(NativeBuffer && SUCCEEDED(NativeBuffer->GetDesc(&BufferDesc)), "Shader Constant Buffer Reflection을 읽지 못했다. Index={}", BufferIndex);

		D3D11_SHADER_INPUT_BIND_DESC BindingDesc = {};
		panicf(SUCCEEDED(NativeReflection->GetResourceBindingDescByName(BufferDesc.Name, &BindingDesc)),
			"Shader Constant Buffer Register를 읽지 못했다. Name={}", BufferDesc.Name);

		FShaderConstantBufferDesc Buffer;
		Buffer.Name = FName(BufferDesc.Name);
		Buffer.Stage = Stage;
		Buffer.Slot = BindingDesc.BindPoint;
		Buffer.Size = BufferDesc.Size;
		Buffer.Parameters.reserve(BufferDesc.Variables);
		for (uint32 VariableIndex = 0; VariableIndex < BufferDesc.Variables; ++VariableIndex)
		{
			ID3D11ShaderReflectionVariable* NativeVariable = NativeBuffer->GetVariableByIndex(VariableIndex);
			D3D11_SHADER_VARIABLE_DESC VariableDesc = {};
			panicf(NativeVariable && SUCCEEDED(NativeVariable->GetDesc(&VariableDesc)),
				"Shader Constant 변수 Reflection을 읽지 못했다. Buffer={}, Index={}", BufferDesc.Name, VariableIndex);

			D3D11_SHADER_TYPE_DESC TypeDesc = {};
			ID3D11ShaderReflectionType* NativeType = NativeVariable->GetType();
			panicf(NativeType && SUCCEEDED(NativeType->GetDesc(&TypeDesc)),
				"Shader Constant 변수 Type Reflection을 읽지 못했다. Buffer={}, Variable={}", BufferDesc.Name, VariableDesc.Name);

			FShaderParameterDesc Parameter;
			Parameter.Name = FName(VariableDesc.Name);
			Parameter.Offset = VariableDesc.StartOffset;
			Parameter.Size = VariableDesc.Size;
			Parameter.Rows = TypeDesc.Rows;
			Parameter.Columns = TypeDesc.Columns;
			Parameter.Elements = TypeDesc.Elements;
			switch (TypeDesc.Type)
			{
			case D3D_SVT_FLOAT: Parameter.BaseType = EShaderParameterBaseType::Float; break;
			case D3D_SVT_INT: Parameter.BaseType = EShaderParameterBaseType::Int; break;
			case D3D_SVT_UINT: Parameter.BaseType = EShaderParameterBaseType::UInt; break;
			case D3D_SVT_BOOL: Parameter.BaseType = EShaderParameterBaseType::Bool; break;
			default: break;
			}
			switch (TypeDesc.Class)
			{
			case D3D_SVC_SCALAR: Parameter.Class = EShaderParameterClass::Scalar; break;
			case D3D_SVC_VECTOR: Parameter.Class = EShaderParameterClass::Vector; break;
			case D3D_SVC_MATRIX_ROWS: Parameter.Class = EShaderParameterClass::MatrixRows; break;
			case D3D_SVC_MATRIX_COLUMNS: Parameter.Class = EShaderParameterClass::MatrixColumns; break;
			case D3D_SVC_STRUCT: Parameter.Class = EShaderParameterClass::Struct; break;
			default: break;
			}
			Buffer.Parameters.push_back(std::move(Parameter));
		}
		Reflection.ConstantBuffers.push_back(std::move(Buffer));
	}

	Reflection.Resources.reserve(ShaderDesc.BoundResources);
	for (uint32 ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ++ResourceIndex)
	{
		D3D11_SHADER_INPUT_BIND_DESC BindingDesc = {};
		panicf(SUCCEEDED(NativeReflection->GetResourceBindingDesc(ResourceIndex, &BindingDesc)),
			"Shader Resource Reflection을 읽지 못했다. Index={}", ResourceIndex);
		if (BindingDesc.Type == D3D_SIT_CBUFFER)
		{
			continue;
		}

		FShaderResourceBindingDesc Resource;
		Resource.Name = FName(BindingDesc.Name);
		Resource.Stage = Stage;
		Resource.Slot = BindingDesc.BindPoint;
		Resource.Count = BindingDesc.BindCount;
		if (BindingDesc.Type == D3D_SIT_SAMPLER)
		{
			Resource.Type = EShaderResourceType::Sampler;
		}
		else if (BindingDesc.Type == D3D_SIT_TEXTURE && BindingDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
		{
			Resource.Type = EShaderResourceType::Texture2D;
		}
		else if (BindingDesc.Type == D3D_SIT_TEXTURE && BindingDesc.Dimension == D3D_SRV_DIMENSION_TEXTURECUBE)
		{
			Resource.Type = EShaderResourceType::TextureCube;
		}
		Reflection.Resources.push_back(std::move(Resource));
	}
	return Reflection;
}
