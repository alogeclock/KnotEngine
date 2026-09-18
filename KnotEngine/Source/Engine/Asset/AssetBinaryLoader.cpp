#include "Asset/AssetBinaryLoader.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetRegistry.h"
#include "Asset/Asset/AssetTypes.h"
#include "Asset/Material/Material.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Texture/Texture2D.h"
#include "Core/Log.h"
#include "Core/MemoryArchive.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

TArray<uint8> FAssetBinaryLoader::LoadAssetFile(const FAssetData& Asset)
{
	if (!Asset.AssetId.IsValid() || !Asset.HasBinaryFile())
	{
		return {};
	}
	std::ifstream Stream(Asset.BinaryFilePath, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		return {};
	}
	const std::streamoff FileSize = Stream.tellg();
	if (FileSize <= 0 || static_cast<uint64>(FileSize) > (std::numeric_limits<SIZE_T>::max)())
	{
		return {};
	}
	TArray<uint8> Bytes(static_cast<SIZE_T>(FileSize));
	Stream.seekg(0, std::ios::beg);
	return Stream.read(reinterpret_cast<char*>(Bytes.data()), FileSize) ? std::move(Bytes) : TArray<uint8>{};
}

bool FAssetBinaryLoader::ReadAssetHeader(
	FMemoryReader& Reader,
	SIZE_T FileSize,
	const FAssetData& Asset,
	EAssetType ExpectedType,
	uint32 ExpectedPayloadVersion,
	FAssetFileHeader& OutHeader)
{
	Reader << OutHeader;
	return !Reader.HasError() && std::memcmp(OutHeader.Magic, FAssetFileHeader::MagicValue, sizeof(OutHeader.Magic)) == 0 &&
		OutHeader.ContainerVersion == FAssetFileHeader::CurrentVersion && OutHeader.AssetType == ExpectedType &&
		OutHeader.PayloadVersion == ExpectedPayloadVersion && OutHeader.AssetId == Asset.AssetId && FileSize >= sizeof(FAssetFileHeader) &&
		OutHeader.PayloadSize == FileSize - sizeof(FAssetFileHeader);
}

// Texture2D Payload의 Mip과 Color Space 정보를 역직렬화한다.
UTexture2D* FAssetBinaryLoader::LoadTexture2D(const FAssetData& Asset) const
{
	const TArray<uint8> FileBytes = LoadAssetFile(Asset);
	FMemoryReader Reader(FileBytes);
	FAssetFileHeader AssetHeader = {};
	if (!ReadAssetHeader(Reader, FileBytes.size(), Asset, EAssetType::Texture2D, FTexture2DPayloadHeader::CurrentVersion, AssetHeader))
	{
		return nullptr;
	}
	FTexture2DPayloadHeader Header = {};
	Reader << Header;
	if (Reader.HasError() || Header.Width == 0 || Header.Height == 0 || Header.MipCount == 0 || Header.MipCount > 32 ||
		Header.Format > ETextureFormat::BC7UNorm || Header.ColorSpace > ETextureColorSpace::SRGB)
	{
		return nullptr;
	}

	TArray<FTextureMipData> Mips;
	Mips.reserve(Header.MipCount);
	for (uint32 MipIndex = 0; MipIndex < Header.MipCount; ++MipIndex)
	{
		FTextureMipPayloadHeader MipHeader = {};
		Reader << MipHeader;
		if (Reader.HasError() || MipHeader.DataSize == 0 || MipHeader.RowPitch == 0 || MipHeader.SlicePitch != MipHeader.DataSize ||
			!Reader.CanSerialize(MipHeader.DataSize))
		{
			return nullptr;
		}
		FTextureMipData Mip;
		Mip.Bytes.resize(MipHeader.DataSize);
		Mip.RowPitch = MipHeader.RowPitch;
		Reader.Serialize(Mip.Bytes.data(), static_cast<int64>(Mip.Bytes.size()));
		if (Reader.HasError())
		{
			return nullptr;
		}
		Mips.push_back(std::move(Mip));
	}
	if (Reader.HasError() || Reader.CanSerialize(1))
	{
		return nullptr;
	}

	UTexture2D* Texture = GUObjectManager.Create<UTexture2D>();
	if (!Texture->Initialize(AssetHeader.AssetId, Asset.AssetPath, Header.Width, Header.Height, Header.Format, Header.ColorSpace, std::move(Mips)))
	{
		GUObjectManager.Destroy(Texture);
		return nullptr;
	}
	return Texture;
}

// Material Payload를 역직렬화하고 참조 Texture를 Asset Manager에서 해결한다.
UMaterial* FAssetBinaryLoader::LoadMaterial(const FAssetData& Asset, FAssetManager& AssetManager) const
{
	const FString& AssetPath = Asset.AssetPath;
	const TArray<uint8> FileBytes = LoadAssetFile(Asset);
	FMemoryReader Reader(FileBytes);
	FAssetFileHeader AssetHeader = {};
	if (!ReadAssetHeader(Reader, FileBytes.size(), Asset, EAssetType::Material, FMaterialPayloadHeader::CurrentVersion, AssetHeader))
	{
		KE_LOG(LogAssetBinaryLoader, Error, "Material Header 검증에 실패했다. AssetPath={}, FileSize={}", AssetPath, FileBytes.size());
		return nullptr;
	}
	FMaterialPayloadHeader Header = {};
	FShaderKey VertexShader;
	FShaderKey PixelShader;
	Reader << Header;
	Reader << VertexShader;
	Reader << PixelShader;
	if (Reader.HasError() ||
		Header.BlendMode > EMaterialBlendMode::Translucent || Header.DepthMode > EMaterialDepthMode::Disabled || Header.CullMode > ECullMode::Back ||
		Header.ScalarParameterCount > 65535 || Header.VectorParameterCount > 65535 || Header.TextureParameterCount > 65535)
	{
		KE_LOG(LogAssetBinaryLoader, Error, "Material Payload Header 검증에 실패했다. AssetPath={}", AssetPath);
		return nullptr;
	}

	TArray<FScalarMaterialParameter> Scalars;
	TArray<FVectorMaterialParameter> Vectors;
	TArray<FTextureMaterialParameter> Textures;
	Scalars.reserve(Header.ScalarParameterCount);
	Vectors.reserve(Header.VectorParameterCount);
	Textures.reserve(Header.TextureParameterCount);
	for (uint32 Index = 0; Index < Header.ScalarParameterCount; ++Index)
	{
		FString Name;
		float Value = 0.0f;
		Reader << Name;
		Reader << Value;
		if (Reader.HasError() || Name.empty())
		{
			return nullptr;
		}
		Scalars.push_back({ FName(Name), Value });
	}
	for (uint32 Index = 0; Index < Header.VectorParameterCount; ++Index)
	{
		FString Name;
		FVector4 Value;
		Reader << Name;
		Reader << Value;
		if (Reader.HasError() || Name.empty())
		{
			return nullptr;
		}
		Vectors.push_back({ FName(Name), Value });
	}
	for (uint32 Index = 0; Index < Header.TextureParameterCount; ++Index)
	{
		FString Name;
		FAssetId TextureId;
		FSamplerDesc Sampler;
		Reader << Name;
		Reader << TextureId;
		Reader << Sampler;
		if (Reader.HasError() || Name.empty() || !TextureId.IsValid() || Sampler.Filter > ESamplerFilter::Anisotropic ||
			Sampler.AddressU > ESamplerAddressMode::Border || Sampler.AddressV > ESamplerAddressMode::Border ||
			Sampler.AddressW > ESamplerAddressMode::Border)
		{
			return nullptr;
		}
		UTexture2D* Texture = AssetManager.LoadTexture2D(TextureId);
		if (!Texture)
		{
			return nullptr;
		}
		Textures.push_back({ FName(Name), Texture, Sampler });
	}
	if (Reader.HasError() || Reader.CanSerialize(1))
	{
		KE_LOG(LogAssetBinaryLoader, Error, "Material Payload 크기 검증에 실패했다. AssetPath={}", AssetPath);
		return nullptr;
	}

	FMaterial RenderMaterial;
	if (!RenderMaterial.Initialize(std::move(VertexShader), std::move(PixelShader), Header.BlendMode, Header.DepthMode, Header.CullMode))
	{
		KE_LOG(LogAssetBinaryLoader, Error, "Material Render Definition 초기화에 실패했다. AssetPath={}", AssetPath);
		return nullptr;
	}
	UMaterial* Material = GUObjectManager.Create<UMaterial>();
	if (!Material->Initialize(AssetHeader.AssetId, AssetPath, std::move(RenderMaterial), std::move(Scalars), std::move(Vectors), std::move(Textures)))
	{
		KE_LOG(LogAssetBinaryLoader, Error, "Material UObject 초기화에 실패했다. AssetPath={}", AssetPath);
		GUObjectManager.Destroy(Material);
		return nullptr;
	}
	return Material;
}

// Static Mesh Payload를 역직렬화하고 참조 Material을 Asset Manager에서 해결한다.
UStaticMesh* FAssetBinaryLoader::LoadStaticMesh(const FAssetData& Asset, FAssetManager& AssetManager) const
{
	const FString& AssetPath = Asset.AssetPath;
	const TArray<uint8> FileBytes = LoadAssetFile(Asset);
	FMemoryReader Reader(FileBytes);
	FAssetFileHeader AssetHeader = {};
	if (!ReadAssetHeader(Reader, FileBytes.size(), Asset, EAssetType::StaticMesh, FStaticMeshPayloadHeader::CurrentVersion, AssetHeader))
	{
		return nullptr;
	}
	FStaticMeshPayloadHeader Header = {};
	Reader << Header;
	if (Reader.HasError() || Header.VertexStride != sizeof(FStaticMeshVertex) || Header.LODCount == 0 ||
		Header.LODCount > FStaticMeshPayloadHeader::MaxLODCount || Header.MaterialCount > 65535)
	{
		return nullptr;
	}

	TArray<FStaticMaterial> Materials;
	Materials.reserve(Header.MaterialCount);
	for (uint32 Index = 0; Index < Header.MaterialCount; ++Index)
	{
		FString SlotName;
		FAssetId MaterialId;
		Reader << SlotName;
		Reader << MaterialId;
		if (Reader.HasError() || SlotName.empty())
		{
			return nullptr;
		}
		UMaterial* Material = MaterialId.IsValid() ? AssetManager.LoadMaterial(MaterialId) : nullptr;
		if (MaterialId.IsValid() && !Material)
		{
			KE_LOG(LogAssetBinaryLoader, Error, "Static Mesh Material을 불러오지 못했다. AssetPath={}, MaterialId={}", AssetPath, MaterialId.ToString());
			return nullptr;
		}
		Materials.push_back({ FName(SlotName), Material });
	}

	FStaticMesh RenderData;
	for (uint32 LODIndex = 0; LODIndex < Header.LODCount; ++LODIndex)
	{
		FStaticMeshLODPayloadHeader LODHeader = {};
		Reader << LODHeader;
		if (Reader.HasError() || LODHeader.VertexCount == 0 || LODHeader.SectionCount == 0 ||
			!Reader.CanSerialize(static_cast<int64>(LODHeader.VertexCount) * sizeof(FStaticMeshVertex)))
		{
			return nullptr;
		}
		TArray<FStaticMeshVertex> Vertices(LODHeader.VertexCount);
		Reader.Serialize(Vertices.data(), static_cast<int64>(Vertices.size() * sizeof(FStaticMeshVertex)));
		if (Reader.HasError() || !Reader.CanSerialize(static_cast<int64>(LODHeader.IndexCount) * sizeof(uint32)))
		{
			return nullptr;
		}
		TArray<uint32> Indices(LODHeader.IndexCount);
		Reader.Serialize(Indices.data(), static_cast<int64>(Indices.size() * sizeof(uint32)));
		if (Reader.HasError() || !Reader.CanSerialize(static_cast<int64>(LODHeader.SectionCount) * sizeof(FStaticMeshSection)))
		{
			return nullptr;
		}
		TArray<FStaticMeshSection> Sections(LODHeader.SectionCount);
		Reader.Serialize(Sections.data(), static_cast<int64>(Sections.size() * sizeof(FStaticMeshSection)));
		if (Reader.HasError())
		{
			return nullptr;
		}
		for (uint32 Index : Indices)
		{
			if (Index >= Vertices.size())
			{
				return nullptr;
			}
		}
		for (const FStaticMeshSection& Section : Sections)
		{
			if (Section.FirstIndex > Indices.size() || Section.IndexCount > Indices.size() - Section.FirstIndex || Section.MaterialIndex >= Materials.size())
			{
				return nullptr;
			}
		}
		if (!RenderData.AddLOD(Vertices, Indices, Sections))
		{
			return nullptr;
		}
	}
	if (Reader.HasError() || Reader.CanSerialize(1))
	{
		return nullptr;
	}

	UStaticMesh* Mesh = GUObjectManager.Create<UStaticMesh>();
	if (!Mesh->Initialize(AssetHeader.AssetId, AssetPath, std::move(RenderData), std::move(Materials)))
	{
		GUObjectManager.Destroy(Mesh);
		return nullptr;
	}
	return Mesh;
}
