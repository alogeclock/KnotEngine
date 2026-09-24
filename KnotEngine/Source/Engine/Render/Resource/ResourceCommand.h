#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/AssetId.h"
#include "Asset/Material/Material.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Texture/Texture2D.h"
#include "Render/RHI/RenderTypes.h"

// Static Mesh Asset의 한 Revision을 GPU Resource로 갱신하는 값 명령이다.
struct ENGINE_API FStaticMeshResourceCommand
{
	FAssetId AssetId;
	uint64 Revision = 0;
	FStaticMesh Mesh;
};

// Texture Asset의 한 Revision을 GPU Resource로 갱신하는 값 명령이다.
struct ENGINE_API FTextureResourceCommand
{
	FAssetId AssetId;
	uint64 Revision = 0;
	FTextureDesc Desc;
	TArray<FTextureMipData> Mips;
};

// Material Parameter가 참조하는 Texture와 Sampler 값이다.
struct ENGINE_API FMaterialTextureData
{
	FName Name;
	FAssetId AssetId;
	FSamplerDesc Sampler;
};

// Material의 현재 값과 Revision을 RT에 전달하며 Shader Reload 시에도 같은 Revision의 후보 데이터로 사용한다.
struct ENGINE_API FMaterialResourceCommand
{
	FAssetId AssetId;
	uint64 Revision = 0;
	FMaterial Material;
	TArray<FScalarMaterialParameter> ScalarParameters;
	TArray<FVectorMaterialParameter> VectorParameters;
	TArray<FMaterialTextureData> Textures;
};
