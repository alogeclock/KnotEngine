#include "Asset/AssetBinaryLoader.h"

#include "Asset/AssetManager.h"
#include "Asset/AssetTypes.h"
#include "Asset/Material/Material.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Texture/Texture2D.h"
#include "Core/IO/Paths.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace
{
class FAssetReader
{
public:
	explicit FAssetReader(TArray<uint8> InBytes)
		: Bytes(std::move(InBytes))
	{
	}

	template <typename T>
	bool Read(T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return ReadBytes(&Value, sizeof(T));
	}

	bool ReadBytes(void* Destination, SIZE_T Size)
	{
		if (Offset > Bytes.size() || Size > Bytes.size() - Offset)
		{
			return false;
		}
		std::memcpy(Destination, Bytes.data() + Offset, Size);
		Offset += Size;
		return true;
	}

	bool ReadString(FString& Value)
	{
		uint32 Length = 0;
		if (!Read(Length) || Length > 65535 || Length > Remaining())
		{
			return false;
		}
		Value.resize(Length);
		return Length == 0 || ReadBytes(Value.data(), Length);
	}

	SIZE_T Remaining() const { return Bytes.size() - Offset; }
	bool IsAtEnd() const { return Offset == Bytes.size(); }

private:
	TArray<uint8> Bytes;
	SIZE_T Offset = 0;
};

TArray<uint8> LoadAssetFile(const FString& AssetPath)
{
	if (AssetPath.size() < 2 || AssetPath.front() != '/' || AssetPath.find("..") != FString::npos)
	{
		return {};
	}
	const std::filesystem::path FilePath = std::filesystem::path(FPaths::ContentDir()) / FPaths::ToWide(AssetPath.substr(1) + ".kasset");
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
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

bool ReadAssetHeader(FAssetReader& Reader, EAssetType ExpectedType, uint32 ExpectedPayloadVersion)
{
	FAssetFileHeader Header;
	return Reader.Read(Header) && std::memcmp(Header.Magic, FAssetFileHeader::MagicValue, sizeof(Header.Magic)) == 0 &&
		Header.ContainerVersion == FAssetFileHeader::CurrentVersion && Header.AssetType == ExpectedType &&
		Header.PayloadVersion == ExpectedPayloadVersion && Header.PayloadSize == Reader.Remaining();
}

bool ReadShaderKey(FAssetReader& Reader, FShaderKey& Key)
{
	return Reader.ReadString(Key.SourcePath) && Reader.ReadString(Key.EntryPoint) && Reader.Read(Key.Stage) && Reader.Read(Key.PermutationId);
}
} // namespace

// Texture2D Payload의 Mip과 Color Space 정보를 역직렬화한다.
UTexture2D* FAssetBinaryLoader::LoadTexture2D(const FString& AssetPath) const
{
	FAssetReader Reader(LoadAssetFile(AssetPath));
	if (!ReadAssetHeader(Reader, EAssetType::Texture2D, FTexture2DPayloadHeader::CurrentVersion))
	{
		return nullptr;
	}
	FTexture2DPayloadHeader Header;
	if (!Reader.Read(Header) || Header.Width == 0 || Header.Height == 0 || Header.MipCount == 0 || Header.MipCount > 32 ||
		Header.Format == ETextureFormat::D24UNormS8UInt || Header.ColorSpace > ETextureColorSpace::SRGB)
	{
		return nullptr;
	}

	TArray<FTextureMipData> Mips;
	Mips.reserve(Header.MipCount);
	for (uint32 MipIndex = 0; MipIndex < Header.MipCount; ++MipIndex)
	{
		FTextureMipPayloadHeader MipHeader;
		if (!Reader.Read(MipHeader) || MipHeader.DataSize == 0 || MipHeader.RowPitch == 0 || MipHeader.SlicePitch != MipHeader.DataSize ||
			MipHeader.DataSize > Reader.Remaining())
		{
			return nullptr;
		}
		FTextureMipData Mip;
		Mip.Bytes.resize(MipHeader.DataSize);
		Mip.RowPitch = MipHeader.RowPitch;
		if (!Reader.ReadBytes(Mip.Bytes.data(), Mip.Bytes.size()))
		{
			return nullptr;
		}
		Mips.push_back(std::move(Mip));
	}
	if (!Reader.IsAtEnd())
	{
		return nullptr;
	}

	UTexture2D* Texture = GUObjectManager.Create<UTexture2D>();
	if (!Texture->Initialize(AssetPath, Header.Width, Header.Height, Header.Format, Header.ColorSpace, std::move(Mips)))
	{
		GUObjectManager.Destroy(Texture);
		return nullptr;
	}
	return Texture;
}

// Material Payload를 역직렬화하고 참조 Texture를 Asset Manager에서 해결한다.
UMaterial* FAssetBinaryLoader::LoadMaterial(const FString& AssetPath, FAssetManager& AssetManager) const
{
	FAssetReader Reader(LoadAssetFile(AssetPath));
	if (!ReadAssetHeader(Reader, EAssetType::Material, FMaterialPayloadHeader::CurrentVersion))
	{
		return nullptr;
	}
	FMaterialPayloadHeader Header;
	FShaderKey VertexShader;
	FShaderKey PixelShader;
	if (!Reader.Read(Header) || !ReadShaderKey(Reader, VertexShader) || !ReadShaderKey(Reader, PixelShader) ||
		Header.BlendMode > EMaterialBlendMode::Translucent || Header.DepthMode > EMaterialDepthMode::Disabled || Header.CullMode > ECullMode::None ||
		Header.ScalarParameterCount > 65535 || Header.VectorParameterCount > 65535 || Header.TextureParameterCount > 65535)
	{
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
		if (!Reader.ReadString(Name) || Name.empty() || !Reader.Read(Value))
		{
			return nullptr;
		}
		Scalars.push_back({ FName(Name), Value });
	}
	for (uint32 Index = 0; Index < Header.VectorParameterCount; ++Index)
	{
		FString Name;
		FVector4 Value;
		if (!Reader.ReadString(Name) || Name.empty() || !Reader.Read(Value))
		{
			return nullptr;
		}
		Vectors.push_back({ FName(Name), Value });
	}
	for (uint32 Index = 0; Index < Header.TextureParameterCount; ++Index)
	{
		FString Name;
		FString TexturePath;
		FSamplerDesc Sampler;
		if (!Reader.ReadString(Name) || Name.empty() || !Reader.ReadString(TexturePath) || TexturePath.empty() || !Reader.Read(Sampler))
		{
			return nullptr;
		}
		UTexture2D* Texture = AssetManager.LoadTexture2D(TexturePath);
		if (!Texture)
		{
			return nullptr;
		}
		Textures.push_back({ FName(Name), Texture, Sampler });
	}
	if (!Reader.IsAtEnd())
	{
		return nullptr;
	}

	FMaterial RenderMaterial;
	if (!RenderMaterial.Initialize(std::move(VertexShader), std::move(PixelShader), Header.BlendMode, Header.DepthMode, Header.CullMode))
	{
		return nullptr;
	}
	UMaterial* Material = GUObjectManager.Create<UMaterial>();
	if (!Material->Initialize(AssetPath, std::move(RenderMaterial), std::move(Scalars), std::move(Vectors), std::move(Textures)))
	{
		GUObjectManager.Destroy(Material);
		return nullptr;
	}
	return Material;
}

// Static Mesh Payload를 역직렬화하고 참조 Material을 Asset Manager에서 해결한다.
UStaticMesh* FAssetBinaryLoader::LoadStaticMesh(const FString& AssetPath, FAssetManager& AssetManager) const
{
	FAssetReader Reader(LoadAssetFile(AssetPath));
	if (!ReadAssetHeader(Reader, EAssetType::StaticMesh, FStaticMeshPayloadHeader::CurrentVersion))
	{
		return nullptr;
	}
	FStaticMeshPayloadHeader Header;
	if (!Reader.Read(Header) || Header.VertexStride != sizeof(FStaticMeshVertex) || Header.LODCount == 0 ||
		Header.LODCount > FStaticMeshPayloadHeader::MaxLODCount || Header.MaterialCount > 65535)
	{
		return nullptr;
	}

	TArray<FStaticMaterial> Materials;
	Materials.reserve(Header.MaterialCount);
	for (uint32 Index = 0; Index < Header.MaterialCount; ++Index)
	{
		FString SlotName;
		FString MaterialPath;
		if (!Reader.ReadString(SlotName) || SlotName.empty() || !Reader.ReadString(MaterialPath))
		{
			return nullptr;
		}
		UMaterial* Material = MaterialPath.empty() ? nullptr : AssetManager.LoadMaterial(MaterialPath);
		if (!MaterialPath.empty() && !Material)
		{
			return nullptr;
		}
		Materials.push_back({ FName(SlotName), Material });
	}

	FStaticMesh RenderData;
	for (uint32 LODIndex = 0; LODIndex < Header.LODCount; ++LODIndex)
	{
		FStaticMeshLODPayloadHeader LODHeader;
		if (!Reader.Read(LODHeader) || LODHeader.VertexCount == 0 || LODHeader.SectionCount == 0 ||
			LODHeader.VertexCount > Reader.Remaining() / sizeof(FStaticMeshVertex))
		{
			return nullptr;
		}
		TArray<FStaticMeshVertex> Vertices(LODHeader.VertexCount);
		if (!Reader.ReadBytes(Vertices.data(), Vertices.size() * sizeof(FStaticMeshVertex)) ||
			LODHeader.IndexCount > Reader.Remaining() / sizeof(uint32))
		{
			return nullptr;
		}
		TArray<uint32> Indices(LODHeader.IndexCount);
		if ((!Indices.empty() && !Reader.ReadBytes(Indices.data(), Indices.size() * sizeof(uint32))) ||
			LODHeader.SectionCount > Reader.Remaining() / sizeof(FStaticMeshSection))
		{
			return nullptr;
		}
		TArray<FStaticMeshSection> Sections(LODHeader.SectionCount);
		if (!Reader.ReadBytes(Sections.data(), Sections.size() * sizeof(FStaticMeshSection)))
		{
			return nullptr;
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
	if (!Reader.IsAtEnd())
	{
		return nullptr;
	}

	UStaticMesh* Mesh = GUObjectManager.Create<UStaticMesh>();
	if (!Mesh->Initialize(AssetPath, std::move(RenderData), std::move(Materials)))
	{
		GUObjectManager.Destroy(Mesh);
		return nullptr;
	}
	return Mesh;
}
