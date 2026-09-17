#pragma once

#include "EngineAPI.h"

#include "Render/Shader/ShaderRegistry.h"

// Surface Material이 Frame Buffer에 색을 합성하는 방식을 정의한다.
enum class EMaterialBlendMode : uint8
{
	Opaque,
	Masked,
	Translucent
};

// Surface Material의 Depth Test와 Depth Write 정책을 정의한다.
enum class EMaterialDepthMode : uint8
{
	ReadWrite,
	ReadOnly,
	Disabled
};

// Material Asset 값으로 채울 수 있는 MaterialConstants 변수 형태다.
enum class EMaterialParameterType : uint8
{
	Scalar,
	Vector2,
	Vector3,
	Vector4
};

// MaterialConstants 안에서 이름으로 찾은 Parameter의 패킹 위치다.
struct ENGINE_API FMaterialParameterDesc
{
	FName Name;
	EMaterialParameterType Type = EMaterialParameterType::Scalar;
	uint32 Offset = 0;
	uint32 Size = 0;
};

// 같은 MaterialConstants 데이터를 Shader Stage의 b Register에 바인딩하는 위치다.
struct ENGINE_API FMaterialConstantBufferBinding
{
	EShaderStage Stage = EShaderStage::Vertex;
	uint32 Slot = 0;
};

// Material Texture와 함께 사용할 t/s Register 위치다. SamplerSlot이 InvalidIndex이면 Sampler를 바인딩하지 않는다.
struct ENGINE_API FMaterialTextureBinding
{
	FName Name;
	EShaderStage Stage = EShaderStage::Pixel;
	uint32 TextureSlot = 0;
	uint32 SamplerSlot = FSamplerHandle::InvalidIndex;
};

// Shader Reflection에서 생성되어 Material Parameter 값의 Constant Buffer 패킹과 Texture 바인딩을 규정한다.
struct ENGINE_API FMaterialParameterLayout
{
	TArray<FMaterialParameterDesc> Parameters; // Parameter
	TArray<FMaterialConstantBufferBinding> ConstantBuffers; // b Register
	TArray<FMaterialTextureBinding> Textures; // t/s Register
	uint32 ConstantBufferSize = 0;
	bool bInitialized = false;
};

// Material Asset이 공유하는 Shader와 고정 Pipeline 상태를 나타내는 렌더 정의다.
class ENGINE_API FMaterial final
{
public:
	FMaterial() = default;

	bool Initialize(
	    FShaderKey InVertexShader,
	    FShaderKey InPixelShader,
	    EMaterialBlendMode InBlendMode = EMaterialBlendMode::Opaque,
	    EMaterialDepthMode InDepthMode = EMaterialDepthMode::ReadWrite,
	    ECullMode InCullMode = ECullMode::Back);

	const FShaderKey& GetVertexShader() const { return VertexShader; }
	const FShaderKey& GetPixelShader() const { return PixelShader; }

	EMaterialBlendMode GetBlendMode() const { return BlendMode; }
	EMaterialDepthMode GetDepthMode() const { return DepthMode; }
	ECullMode GetCullMode() const { return CullMode; }

	const FMaterialParameterLayout& GetOrCreateParameterLayout(FShaderRegistry& ShaderRegistry) const;

	bool IsValid() const { return !VertexShader.SourcePath.empty() && !PixelShader.SourcePath.empty(); }

private:
	static void AppendShaderReflection(FMaterialParameterLayout& Layout, const FShaderReflection& Reflection);

	FShaderKey VertexShader;
	FShaderKey PixelShader;

	EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
	EMaterialDepthMode DepthMode = EMaterialDepthMode::ReadWrite;
	ECullMode CullMode = ECullMode::Back;

	mutable FMaterialParameterLayout ParameterLayout;
};
