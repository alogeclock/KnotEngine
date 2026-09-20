#pragma once

#include "EngineAPI.h"

#include "Asset/Texture/Texture.h"
#include "Core/Math/Vector4.h"
#include "Core/Name.h"
#include "Object/ObjectPtr.h"
#include "Render/RHI/RenderTypes.h"

// Material Asset 값으로 채울 수 있는 MaterialConstants 변수 형태다.
enum class EMaterialParameterType : uint8
{
	Scalar,
	Vector2,
	Vector3,
	Vector4
};

// Shader Reflection이 만든 Material Parameter의 이름, 형식과 Constant Buffer 패킹 위치다.
struct ENGINE_API FMaterialParameterDesc
{
	FName Name;
	EMaterialParameterType Type = EMaterialParameterType::Scalar;
	uint32 Offset = 0;
	uint32 Size = 0;
};

// 이름으로 식별되는 단일 실수 Material Parameter 값이다.
struct ENGINE_API FScalarMaterialParameter
{
	FName Name;
	float Value = 0.0f;
};

// 이름으로 식별되는 4성분 Material Parameter 값이다.
struct ENGINE_API FVectorMaterialParameter
{
	FName Name;
	FVector4 Value;
};

// 이름으로 식별되는 Texture와 해당 Texture를 읽는 Sampler 상태다.
struct ENGINE_API FTextureMaterialParameter
{
	FName Name;
	TObjectPtr<UTexture> Texture;
	FSamplerDesc Sampler;
};
