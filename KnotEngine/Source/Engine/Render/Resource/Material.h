#pragma once

#include "EngineAPI.h"

#include "Render/Resource/ShaderRegistry.h"

// Surface Material이 Frame Buffer에 색을 합성하는 방식을 정의한다.
enum class EMaterialBlendMode : uint8 { Opaque, Masked, Translucent };

// Surface Material의 Depth Test와 Depth Write 정책을 정의한다.
enum class EMaterialDepthMode : uint8 { ReadWrite, ReadOnly, Disabled };

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
	bool IsValid() const { return !VertexShader.SourcePath.empty() && !PixelShader.SourcePath.empty(); }

private:
	FShaderKey VertexShader;
	FShaderKey PixelShader;
	EMaterialBlendMode BlendMode = EMaterialBlendMode::Opaque;
	EMaterialDepthMode DepthMode = EMaterialDepthMode::ReadWrite;
	ECullMode CullMode = ECullMode::Back;
};
