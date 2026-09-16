#include "Render/Resource/Material.h"

#include <utility>

// Material의 Shader 식별자와 Opaque/Masked/Translucent 공통 Pipeline 상태를 초기화한다.
bool FMaterial::Initialize(
	FShaderKey InVertexShader,
	FShaderKey InPixelShader,
	EMaterialBlendMode InBlendMode,
	EMaterialDepthMode InDepthMode,
	ECullMode InCullMode)
{
	if (InVertexShader.SourcePath.empty() || InVertexShader.EntryPoint.empty() || InVertexShader.Stage != EShaderStage::Vertex ||
		InPixelShader.SourcePath.empty() || InPixelShader.EntryPoint.empty() || InPixelShader.Stage != EShaderStage::Pixel)
	{
		return false;
	}

	VertexShader = std::move(InVertexShader);
	PixelShader = std::move(InPixelShader);
	BlendMode = InBlendMode;
	DepthMode = InDepthMode;
	CullMode = InCullMode;
	return true;
}
