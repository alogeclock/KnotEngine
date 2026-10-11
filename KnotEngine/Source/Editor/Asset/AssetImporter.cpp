#include "Asset/AssetImporter.h"
#include "Asset/MeshOptimizer.h"
#include "Asset/MeshSimplifier.h"

#include "Asset/Asset/EngineAssetIds.h"
#include "Asset/Material/Material.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Texture/Texture.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/Math/Matrix.h"
#include "Core/Archive/MemoryArchive.h"

#include <DirectXTex.h>
#include <bcrypt.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

// Scope를 벗어날 때 cgltf가 할당한 GLB 데이터를 해제한다.
FAssetImporter::FGLTFGuard::~FGLTFGuard()
{
	cgltf_free(Data);
}

// GLB 요소 이름을 Asset 파일명으로 사용할 수 있는 문자열로 정리한다.
FString FAssetImporter::Sanitize(const char* Name, const FString& Fallback)
{
	FString Result = Name && *Name ? Name : Fallback;
	for (char& Character : Result)
	{
		if (Character == '<' || Character == '>' || Character == ':' || Character == '"' || Character == '/' ||
		    Character == '\\' || Character == '|' || Character == '?' || Character == '*')
		{
			Character = '_';
		}
	}
	return Result;
}

// 두 Asset 경로를 하나의 구분자로 연결한다.
FString FAssetImporter::Combine(const FString& Left, const FString& Right)
{
	return Left.ends_with('/') ? Left + Right : Left + "/" + Right;
}

// 공통 Header와 Payload를 임시 파일에 기록한 뒤 최종 .kasset으로 교체한다.
bool FAssetImporter::SaveAsset(const FString& AssetPath, EAssetType Type, uint32 PayloadVersion, const TArray<uint8>& PayloadBytes, FAssetId& SavedAssetId)
{
	const std::filesystem::path FinalPath = FPaths::ResolveContentPath(AssetPath + ".kasset");
	const std::filesystem::path TemporaryPath = FinalPath.wstring() + L".tmp";
	std::error_code Error;
	std::filesystem::create_directories(FinalPath.parent_path(), Error);
	if (Error)
	{
		return false;
	}

	FAssetId AssetId;
	if (AssetPath == "/Engine/Model/Capsule/Capsule")
	{
		AssetId = FEngineAssetIds::Capsule;
	}
	else if (AssetPath == "/Engine/Model/Cube/Cube")
	{
		AssetId = FEngineAssetIds::Cube;
	}
	else if (AssetPath == "/Engine/Model/Cylinder/Cylinder")
	{
		AssetId = FEngineAssetIds::Cylinder;
	}
	else if (AssetPath == "/Engine/Model/Quad/Quad")
	{
		AssetId = FEngineAssetIds::Quad;
	}
	else if (AssetPath == "/Engine/Model/Sphere/Sphere")
	{
		AssetId = FEngineAssetIds::Sphere;
	}
	std::ifstream ExistingStream(FinalPath, std::ios::binary);
	FAssetFileHeader ExistingHeader = {};
	if (!AssetId.IsValid() && ExistingStream.read(reinterpret_cast<char*>(&ExistingHeader), sizeof(ExistingHeader)) &&
	    std::memcmp(ExistingHeader.Magic, FAssetFileHeader::MagicValue, sizeof(ExistingHeader.Magic)) == 0 &&
	    ExistingHeader.ContainerVersion == FAssetFileHeader::CurrentVersion && ExistingHeader.AssetId.IsValid())
	{
		AssetId = ExistingHeader.AssetId;
	}
	else if (!AssetId.IsValid())
	{
		AssetId = FAssetId::New();
	}
	ExistingStream.close();

	FAssetFileHeader Header = {};
	std::memcpy(Header.Magic, FAssetFileHeader::MagicValue, sizeof(Header.Magic));
	Header.ContainerVersion = FAssetFileHeader::CurrentVersion;
	Header.AssetType = Type;
	Header.PayloadVersion = PayloadVersion;
	Header.PayloadSize = PayloadBytes.size();
	Header.AssetId = AssetId;
	TArray<uint8> FileBytes;
	FMemoryWriter FileArchive(FileBytes);
	FileArchive << Header;
	FileArchive.Serialize(const_cast<uint8*>(PayloadBytes.data()), static_cast<int64>(PayloadBytes.size()));
	if (FileArchive.HasError())
	{
		return false;
	}
	std::ofstream Stream(TemporaryPath, std::ios::binary | std::ios::trunc);
	if (!Stream || !Stream.write(reinterpret_cast<const char*>(FileBytes.data()), static_cast<std::streamsize>(FileBytes.size())))
	{
		return false;
	}
	Stream.close();
	std::filesystem::remove(FinalPath, Error);
	Error.clear();
	std::filesystem::rename(TemporaryPath, FinalPath, Error);
	if (Error)
	{
		return false;
	}
	SavedAssetId = AssetId;
	return true;
}

// glTF의 오른손 Y-Up 좌표를 미터 단위를 유지하며 Knot Engine의 왼손 Z-Up 좌표로 변환한다.
FVector FAssetImporter::ConvertPosition(const FVector& Value)
{
	return FVector(-Value.Z, Value.X, Value.Y);
}

// glTF 방향 벡터를 Knot Engine 좌표계로 변환하고 정규화한다.
FVector FAssetImporter::ConvertDirection(const FVector& Value)
{
	return FVector(-Value.Z, Value.X, Value.Y).GetSafeNormal();
}

// glTF column-major 행렬을 Knot Engine의 row-vector FMatrix로 변환한다.
FMatrix FAssetImporter::ConvertMatrix(const float Value[16])
{
	return FMatrix(
	    Value[0], Value[1], Value[2], Value[3],
	    Value[4], Value[5], Value[6], Value[7],
	    Value[8], Value[9], Value[10], Value[11],
	    Value[12], Value[13], Value[14], Value[15]);
}

// Position과 UV로 정점 Tangent를 생성하고 Bitangent 방향을 Handedness에 기록한다.
void FAssetImporter::GenerateTangents(TArray<FStaticMeshVertex>& Vertices, const TArray<uint32>& Indices)
{
	TArray<FVector> Tangents(Vertices.size());
	TArray<FVector> Bitangents(Vertices.size());
	for (SIZE_T Index = 0; Index + 2 < Indices.size(); Index += 3)
	{
		const uint32 I0 = Indices[Index];
		const uint32 I1 = Indices[Index + 1];
		const uint32 I2 = Indices[Index + 2];

		const FVector Edge1 = Vertices[I1].Position - Vertices[I0].Position;
		const FVector Edge2 = Vertices[I2].Position - Vertices[I0].Position;

		const FVector2 UV1 = Vertices[I1].TexCoord - Vertices[I0].TexCoord;
		const FVector2 UV2 = Vertices[I2].TexCoord - Vertices[I0].TexCoord;

		const float Determinant = UV1.X * UV2.Y - UV1.Y * UV2.X;

		if (std::fabs(Determinant) <= KMath::Epsilon)
		{
			continue;
		}

		const float Scale = 1.0f / Determinant;
		const FVector Tangent = (Edge1 * UV2.Y - Edge2 * UV1.Y) * Scale;
		const FVector Bitangent = (Edge2 * UV1.X - Edge1 * UV2.X) * Scale;

		Tangents[I0] += Tangent;
		Tangents[I1] += Tangent;
		Tangents[I2] += Tangent;

		Bitangents[I0] += Bitangent;
		Bitangents[I1] += Bitangent;
		Bitangents[I2] += Bitangent;
	}

	for (SIZE_T Index = 0; Index < Vertices.size(); ++Index)
	{
		const FVector Normal = Vertices[Index].Normal.GetSafeNormal();
		FVector Tangent = Tangents[Index] - Normal * (Normal | Tangents[Index]);
		if (!Tangent.Normalize())
		{
			const FVector Axis = std::fabs(Normal.Z) < 0.999f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
			Tangent = (Axis ^ Normal).GetSafeNormal();
		}
		Vertices[Index].Tangent = Tangent;
		Vertices[Index].Handedness = ((Normal ^ Tangent) | Bitangents[Index]) < 0.0f ? -1.0f : 1.0f;
	}
}

// Vertex Bounds의 가장 긴 축이 지정한 크기가 되도록 Mesh를 균일하게 조정한다.
void FAssetImporter::Normalize(TArray<FStaticMeshVertex>& Vertices, float MaximumSize)
{
	if (Vertices.empty())
	{
		return;
	}
	FVector Minimum = Vertices.front().Position;
	FVector Maximum = Minimum;
	for (const FStaticMeshVertex& Vertex : Vertices)
	{
		Minimum.X = std::min(Minimum.X, Vertex.Position.X);
		Minimum.Y = std::min(Minimum.Y, Vertex.Position.Y);
		Minimum.Z = std::min(Minimum.Z, Vertex.Position.Z);
		Maximum.X = std::max(Maximum.X, Vertex.Position.X);
		Maximum.Y = std::max(Maximum.Y, Vertex.Position.Y);
		Maximum.Z = std::max(Maximum.Z, Vertex.Position.Z);
	}
	const FVector Size = Maximum - Minimum;
	const float LargestDimension = std::max({ Size.X, Size.Y, Size.Z });
	if (LargestDimension <= KMath::Epsilon)
	{
		return;
	}
	const float Scale = MaximumSize / LargestDimension;
	for (FStaticMeshVertex& Vertex : Vertices)
	{
		Vertex.Position *= Scale;
	}
}

// Primitive에서 지정한 Semantic Type과 Index에 해당하는 Attribute Accessor를 찾는다.
const cgltf_accessor* FAssetImporter::FindAttribute(const cgltf_primitive& Primitive, int Type, int Index)
{
	for (cgltf_size AttributeIndex = 0; AttributeIndex < Primitive.attributes_count; ++AttributeIndex)
	{
		const cgltf_attribute& Attribute = Primitive.attributes[AttributeIndex];
		if (Attribute.type == Type && Attribute.index == Index)
		{
			return Attribute.data;
		}
	}
	return nullptr;
}

// glTF Sampler 설정을 렌더러가 사용하는 Sampler Descriptor로 변환한다.
FSamplerDesc FAssetImporter::ConvertSampler(const cgltf_sampler* Sampler)
{
	FSamplerDesc Result;
	if (!Sampler)
	{
		return Result;
	}
	Result.Filter = Sampler->min_filter == cgltf_filter_type_nearest || Sampler->mag_filter == cgltf_filter_type_nearest
	                    ? ESamplerFilter::Point
	                : Sampler->min_filter == cgltf_filter_type_linear_mipmap_linear ? ESamplerFilter::Trilinear
	                                                                                : ESamplerFilter::Bilinear;
	const auto ConvertAddress = [](cgltf_int Value)
	{
		if (Value == 33071)
		{
			return ESamplerAddressMode::Clamp;
		}
		if (Value == 33648)
		{
			return ESamplerAddressMode::Mirror;
		}
		return ESamplerAddressMode::Wrap;
	};
	Result.AddressU = ConvertAddress(Sampler->wrap_s);
	Result.AddressV = ConvertAddress(Sampler->wrap_t);
	return Result;
}

// 이미지와 변환 설정 또는 압축 Payload의 SHA-256을 계산한다.
bool FAssetImporter::HashTextureData(std::span<const uint8> Bytes, TStaticArray<uint8, 32>& Hash, std::span<const uint8> Settings)
{
	BCRYPT_ALG_HANDLE Algorithm = nullptr;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&Algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
	{
		return false;
	}

	BCRYPT_HASH_HANDLE HashHandle = nullptr;
	bool bSucceeded = BCRYPT_SUCCESS(BCryptCreateHash(Algorithm, &HashHandle, nullptr, 0, nullptr, 0, 0));
	for (const std::span<const uint8> Data : { Settings, Bytes })
	{
		for (SIZE_T Offset = 0; bSucceeded && Offset < Data.size();)
		{
			const ULONG Size = static_cast<ULONG>(std::min(Data.size() - Offset, static_cast<SIZE_T>((std::numeric_limits<ULONG>::max)())));
			bSucceeded = BCRYPT_SUCCESS(BCryptHashData(HashHandle, const_cast<uint8*>(Data.data() + Offset), Size, 0));
			Offset += Size;
		}
	}

	if (bSucceeded)
	{
		bSucceeded = BCRYPT_SUCCESS(BCryptFinishHash(HashHandle, Hash.data(), static_cast<ULONG>(Hash.size()), 0));
	}

	if (HashHandle)
	{
		BCryptDestroyHash(HashHandle);
	}

	BCryptCloseAlgorithmProvider(Algorithm, 0);
	return bSucceeded;
}

// 크기와 SHA-256이 유효한 캐시에서 Texture Payload를 읽는다.
bool FAssetImporter::LoadTextureCache(const std::filesystem::path& CachePath, TArray<uint8>& PayloadBytes)
{
	static constexpr uint64 MaxPayloadSize = 512ull * 1024 * 1024;
	TStaticArray<uint8, 32> StoredHash;
	std::ifstream Stream(CachePath, std::ios::binary | std::ios::ate);
	
	if (!Stream)
	{
		return false;
	}

	const std::streamoff FileSize = Stream.tellg();
	if (FileSize < static_cast<std::streamoff>(sizeof(FTexture2DPayloadHeader) + StoredHash.size()) ||
		FileSize > static_cast<std::streamoff>(MaxPayloadSize + StoredHash.size()))
	{
		return false;
	}

	TArray<uint8> CachedBytes(static_cast<SIZE_T>(FileSize) - StoredHash.size());
	Stream.seekg(0);
	if (!Stream.read(reinterpret_cast<char*>(CachedBytes.data()), static_cast<std::streamsize>(CachedBytes.size())) ||
		!Stream.read(reinterpret_cast<char*>(StoredHash.data()), static_cast<std::streamsize>(StoredHash.size())))
	{
		return false;
	}

	TStaticArray<uint8, 32> PayloadHash;
	if (!HashTextureData(CachedBytes, PayloadHash) || PayloadHash != StoredHash)
	{
		return false;
	}

	PayloadBytes = std::move(CachedBytes);
	return true;
}

// 압축 Payload와 SHA-256을 임시 파일에 저장한 뒤 기존 캐시를 원자적으로 교체한다.
bool FAssetImporter::SaveTextureCache(const std::filesystem::path& CachePath, const TArray<uint8>& PayloadBytes)
{
	TStaticArray<uint8, 32> PayloadHash;
	if (!HashTextureData(PayloadBytes, PayloadHash))
	{
		return false;
	}

	std::error_code Error;
	std::filesystem::create_directories(CachePath.parent_path(), Error);
	if (Error)
	{
		return false;
	}

	const std::filesystem::path TemporaryPath = CachePath.wstring() + L".tmp";
	std::ofstream Stream(TemporaryPath, std::ios::binary | std::ios::trunc);
	Stream.write(reinterpret_cast<const char*>(PayloadBytes.data()), static_cast<std::streamsize>(PayloadBytes.size()));
	Stream.write(reinterpret_cast<const char*>(PayloadHash.data()), static_cast<std::streamsize>(PayloadHash.size()));
	Stream.close();
	const bool bSucceeded = Stream && MoveFileExW(TemporaryPath.c_str(), CachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	
	if (!bSucceeded)
	{
		std::filesystem::remove(TemporaryPath, Error);
	}

	return bSucceeded;
}

// 압축 캐시를 재사용하거나 Image를 Texture2D .kasset으로 변환하고 Material 연결용 Asset ID를 반환한다.
FAssetImportResult FAssetImporter::ImportTextures(const FTextureImportDesc& Desc)
{
	const cgltf_data& GLTF = Desc.GLTF;
	const FString& TextureRoot = Desc.AssetRoot;
	FAssetImportResult Result;
	// 이미지 변환 방식 또는 인코더를 변경하면 CacheVersion도 올려 기존 결과를 무효화한다.
	static constexpr uint32 CacheVersion = 1;
	static constexpr DirectX::WIC_FLAGS DecodeFlags = DirectX::WIC_FLAGS_FORCE_RGB;
	static constexpr DirectX::TEX_FILTER_FLAGS MipFlags = DirectX::TEX_FILTER_DEFAULT;
	static constexpr DirectX::TEX_COMPRESS_FLAGS CompressFlags = DirectX::TEX_COMPRESS_PARALLEL | DirectX::TEX_COMPRESS_BC7_QUICK;
	static constexpr uint32 CacheSettings[] = {
		CacheVersion, DIRECTX_TEX_VERSION, FTexture2DPayloadHeader::CurrentVersion,
		static_cast<uint32>(DecodeFlags), static_cast<uint32>(MipFlags), 0,
		static_cast<uint32>(DXGI_FORMAT_BC7_UNORM), static_cast<uint32>(CompressFlags),
		std::bit_cast<uint32>(DirectX::TEX_THRESHOLD_DEFAULT), static_cast<uint32>(ETextureColorSpace::SRGB)
	};

	uint32 CachedImageCount = 0;
	for (cgltf_size ImageIndex = 0; ImageIndex < GLTF.images_count; ++ImageIndex)
	{
		const cgltf_image& Image = GLTF.images[ImageIndex];
		if (!Image.buffer_view || !Image.buffer_view->buffer || !Image.buffer_view->buffer->data)
		{
			Result.Error = "외부 URI Image는 현재 지원하지 않는다.";
			return Result;
		}
		const uint8* ImageBytes = static_cast<const uint8*>(Image.buffer_view->buffer->data) + Image.buffer_view->offset;
		const FString TextureName = Sanitize(Image.name, "Texture_" + std::to_string(ImageIndex));
		const FString AssetPath = Combine(TextureRoot, TextureName);
		std::filesystem::path CachePath;
		TStaticArray<uint8, 32> SourceHash;

		if (HashTextureData(std::span<const uint8>(ImageBytes, Image.buffer_view->size), SourceHash,
			std::span<const uint8>(reinterpret_cast<const uint8*>(CacheSettings), sizeof(CacheSettings))))
		{
			static constexpr char HexDigits[] = "0123456789abcdef";
			FString CacheKey;
			CacheKey.reserve(SourceHash.size() * 2);
			for (const uint8 Byte : SourceHash)
			{
				CacheKey.push_back(HexDigits[Byte >> 4]);
				CacheKey.push_back(HexDigits[Byte & 15]);
			}
			CachePath = std::filesystem::path(FPaths::SavedDir()) / L"ImportCache" / L"Texture" / (CacheKey + ".bin");
		}
		else
		{
			Result.Warnings.push_back("Texture 캐시 Key 생성에 실패해 기존 압축 경로를 사용한다: " + AssetPath);
		}

		TArray<uint8> PayloadBytes;
		const bool bCached = !CachePath.empty() && LoadTextureCache(CachePath, PayloadBytes);
		
		if (bCached)
		{
			++CachedImageCount;
		}
		else
		{
			DirectX::ScratchImage Decoded;
			DirectX::ScratchImage MipChain;
			DirectX::ScratchImage Compressed;
			DirectX::TexMetadata Metadata;
			if (FAILED(DirectX::LoadFromWICMemory(ImageBytes, Image.buffer_view->size, DecodeFlags, &Metadata, Decoded)) ||
			    FAILED(DirectX::GenerateMipMaps(Decoded.GetImages(), Decoded.GetImageCount(), Decoded.GetMetadata(), MipFlags, 0, MipChain)) ||
			    FAILED(DirectX::Compress(MipChain.GetImages(), MipChain.GetImageCount(), MipChain.GetMetadata(), DXGI_FORMAT_BC7_UNORM,
			                             CompressFlags, DirectX::TEX_THRESHOLD_DEFAULT, Compressed)))
			{
				Result.Error = "Texture decode, mip 생성 또는 BC7 압축에 실패했다.";
				return Result;
			}

			FMemoryWriter Payload(PayloadBytes);
			FTexture2DPayloadHeader Header = {};
			Header.Width = static_cast<uint32>(Compressed.GetMetadata().width);
			Header.Height = static_cast<uint32>(Compressed.GetMetadata().height);
			Header.MipCount = static_cast<uint32>(Compressed.GetMetadata().mipLevels);
			Header.Format = ETextureFormat::BC7UNorm;
			Header.ColorSpace = ETextureColorSpace::SRGB;
			Payload << Header;
			for (SIZE_T MipIndex = 0; MipIndex < Compressed.GetImageCount(); ++MipIndex)
			{
				const DirectX::Image& Mip = Compressed.GetImages()[MipIndex];
				FTextureMipPayloadHeader MipHeader;
				MipHeader.DataSize = static_cast<uint32>(Mip.slicePitch);
				MipHeader.RowPitch = static_cast<uint32>(Mip.rowPitch);
				MipHeader.SlicePitch = static_cast<uint32>(Mip.slicePitch);
				Payload << MipHeader;
				Payload.Serialize(Mip.pixels, static_cast<int64>(Mip.slicePitch));
			}
			if (Payload.HasError())
			{
				Result.Error = "Texture Payload 생성에 실패했다: " + AssetPath;
				return Result;
			}
			if (!CachePath.empty() && !SaveTextureCache(CachePath, PayloadBytes))
			{
				Result.Warnings.push_back("Texture 압축 캐시 저장에 실패했다: " + AssetPath);
			}
		}
		FAssetId TextureId;
		if (!SaveAsset(AssetPath, EAssetType::Texture2D, FTexture2DPayloadHeader::CurrentVersion, PayloadBytes, TextureId))
		{
			Result.Error = "Texture .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::Texture2D, TextureId });
	}
	KE_LOG(LogAssetImporter, Display, "Texture Import 완료. Images={}, CacheHits={}, Compressed={}", GLTF.images_count, CachedImageCount, GLTF.images_count - CachedImageCount);
	Result.bSucceeded = true;
	return Result;
}

// GLB Material을 Material .kasset으로 변환하고 Static Mesh Slot 연결에 사용할 Asset ID를 반환한다.
FAssetImportResult FAssetImporter::ImportMaterials(const FMaterialImportDesc& Desc)
{
	const cgltf_data& GLTF = Desc.GLTF;
	const FString& MaterialRoot = Desc.AssetRoot;
	const TArray<FAssetId>& TextureAssetIds = Desc.TextureAssetIds;
	FAssetImportResult Result;
	for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.materials_count; ++MaterialIndex)
	{
		const cgltf_material& Source = GLTF.materials[MaterialIndex];
		const bool bMasked = Source.alpha_mode == cgltf_alpha_mode_mask;
		const bool bTranslucent = Source.alpha_mode == cgltf_alpha_mode_blend;
		const FString MaterialName = Sanitize(Source.name, "Material_" + std::to_string(MaterialIndex));
		const FString AssetPath = Combine(MaterialRoot, MaterialName);
		FMaterialPayloadHeader Header = {};
		Header.BlendMode = bTranslucent ? EMaterialBlendMode::Translucent : bMasked ? EMaterialBlendMode::Masked
		                                                                            : EMaterialBlendMode::Opaque;
		Header.DepthMode = EDepthMode::ReadWrite;
		Header.CullMode = Source.double_sided ? ECullMode::None : ECullMode::Back;
		Header.ScalarParameterCount = 1;
		Header.VectorParameterCount = 1;
		Header.TextureParameterCount = Source.has_pbr_metallic_roughness && Source.pbr_metallic_roughness.base_color_texture.texture ? 1 : 0;

		TArray<uint8> PayloadBytes;
		FMemoryWriter Payload(PayloadBytes);
		Payload << Header;
		FShaderKey VertexShader = { "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
		FShaderKey PixelShader = {
			"/Engine/Shader/StaticMesh.hlsl", bTranslucent ? "TranslucentPS" : bMasked ? "MaskedPS"
			                                                                           : "OpaquePS",
			EShaderStage::Pixel
		};
		Payload << VertexShader;
		Payload << PixelShader;
		FString AlphaCutoffName = "AlphaCutoff";
		float AlphaCutoff = Source.alpha_cutoff;
		Payload << AlphaCutoffName;
		Payload << AlphaCutoff;
		FString BaseColorName = "BaseColor";
		Payload << BaseColorName;
		const cgltf_float* Factor = Source.pbr_metallic_roughness.base_color_factor;
		FVector4 BaseColor(Factor[0], Factor[1], Factor[2], Factor[3]);
		Payload << BaseColor;
		if (Header.TextureParameterCount > 0)
		{
			const cgltf_texture_view& View = Source.pbr_metallic_roughness.base_color_texture;
			const SIZE_T ImageIndex = static_cast<SIZE_T>(View.texture->image - GLTF.images);
			FString BaseColorTextureName = "BaseColorTexture";
			FAssetId TextureId = TextureAssetIds[ImageIndex];
			FSamplerDesc Sampler = ConvertSampler(View.texture->sampler);
			Payload << BaseColorTextureName;
			Payload << TextureId;
			Payload << Sampler;
		}
		FAssetId MaterialId;
		if (Payload.HasError() || !SaveAsset(AssetPath, EAssetType::Material, FMaterialPayloadHeader::CurrentVersion, PayloadBytes, MaterialId))
		{
			Result.Error = "Material .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::Material, MaterialId });
	}
	Result.bSucceeded = true;
	return Result;
}

// GLB Mesh와 Scene Transform을 Static Mesh .kasset으로 변환하고 요청한 하위 LOD를 함께 생성한다.
FAssetImportResult FAssetImporter::ImportStaticMeshes(const FStaticMeshImportDesc& Desc)
{
	const cgltf_data& GLTF = Desc.GLTF;
	const std::filesystem::path& SourceFilePath = Desc.SourceFilePath;
	const FString& MeshRoot = Desc.AssetRoot;
	const FGLBImportOptions& ImportOptions = Desc.Options;
	const TArray<FAssetId>& MaterialAssetIds = Desc.MaterialAssetIds;
	FAssetImportResult Result;
	const bool bEngineGeometry = Desc.bEngineGeometry;

	struct FMeshBuildData
	{
		FString Name;
		TArray<FStaticMeshVertex> Vertices;
		TArray<uint32> Indices;
		TArray<FStaticMeshSection> Sections;
	};

	const auto AppendMesh = [&](const cgltf_mesh& SourceMesh, const FMatrix* WorldMatrix, FMeshBuildData& BuildData)
	{
		const float Determinant = WorldMatrix ? WorldMatrix->GetDeterminant() : 1.0f;
		const FMatrix NormalMatrix = WorldMatrix ? WorldMatrix->GetNormalMatrix() : FMatrix::Identity;
		for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < SourceMesh.primitives_count; ++PrimitiveIndex)
		{
			const cgltf_primitive& Primitive = SourceMesh.primitives[PrimitiveIndex];
			if (Primitive.type != cgltf_primitive_type_triangles)
			{
				Result.Error = "Triangle이 아닌 GLB Primitive는 지원하지 않는다.";
				return false;
			}
			const cgltf_accessor* Positions = FindAttribute(Primitive, cgltf_attribute_type_position);
			const cgltf_accessor* Normals = FindAttribute(Primitive, cgltf_attribute_type_normal);
			const cgltf_accessor* TexCoords = FindAttribute(Primitive, cgltf_attribute_type_texcoord, 0);
			if (!Positions || Positions->count == 0 || BuildData.Vertices.size() + Positions->count > (std::numeric_limits<uint32>::max)())
			{
				Result.Error = "POSITION이 없거나 지원 가능한 Vertex 개수를 초과한 GLB Primitive다.";
				return false;
			}
			const uint32 BaseVertex = static_cast<uint32>(BuildData.Vertices.size());
			for (cgltf_size VertexIndex = 0; VertexIndex < Positions->count; ++VertexIndex)
			{
				float Position[3] = {};
				float Normal[3] = { 0.0f, 1.0f, 0.0f };
				float UV[2] = {};
				cgltf_accessor_read_float(Positions, VertexIndex, Position, 3);
				if (Normals)
				{
					cgltf_accessor_read_float(Normals, VertexIndex, Normal, 3);
				}
				if (TexCoords)
				{
					cgltf_accessor_read_float(TexCoords, VertexIndex, UV, 2);
				}
				const FVector SourcePosition(Position[0], Position[1], Position[2]);
				const FVector SourceNormal(Normal[0], Normal[1], Normal[2]);
				const FVector TransformedPosition = WorldMatrix ? WorldMatrix->TransformPosition(SourcePosition) : SourcePosition;
				const FVector TransformedNormal = WorldMatrix ? NormalMatrix.TransformVector(SourceNormal) : SourceNormal;
				FStaticMeshVertex Vertex;
				Vertex.Position = ConvertPosition(TransformedPosition) * ImportOptions.UniformScale;
				Vertex.Normal = ConvertDirection(TransformedNormal);
				Vertex.TexCoord = FVector2(UV[0], UV[1]);
				BuildData.Vertices.push_back(Vertex);
			}

			const uint32 FirstIndex = static_cast<uint32>(BuildData.Indices.size());
			const cgltf_size IndexCount = Primitive.indices ? Primitive.indices->count : Positions->count;
			if (IndexCount % 3 != 0 || BuildData.Indices.size() + IndexCount > (std::numeric_limits<uint32>::max)())
			{
				Result.Error = "Triangle Primitive의 Index 개수가 유효하지 않다.";
				return false;
			}
			const bool bReverseWinding = Determinant >= 0.0f;
			for (cgltf_size Index = 0; Index < IndexCount; Index += 3)
			{
				const auto ReadIndex = [&Primitive](cgltf_size Value)
				{
					return Primitive.indices ? cgltf_accessor_read_index(Primitive.indices, Value) : Value;
				};
				const uint32 I0 = BaseVertex + static_cast<uint32>(ReadIndex(Index));
				const uint32 I1 = BaseVertex + static_cast<uint32>(ReadIndex(Index + 1));
				const uint32 I2 = BaseVertex + static_cast<uint32>(ReadIndex(Index + 2));
				BuildData.Indices.push_back(I0);
				BuildData.Indices.push_back(bReverseWinding ? I2 : I1);
				BuildData.Indices.push_back(bReverseWinding ? I1 : I2);
			}
			const uint32 MaterialIndex = Primitive.material ? static_cast<uint32>(Primitive.material - GLTF.materials) : 0;
			if (!BuildData.Sections.empty() && BuildData.Sections.back().MaterialIndex == MaterialIndex &&
			    BuildData.Sections.back().FirstIndex + BuildData.Sections.back().IndexCount == FirstIndex)
			{
				BuildData.Sections.back().IndexCount += static_cast<uint32>(IndexCount);
			}
			else
			{
				BuildData.Sections.push_back({ FirstIndex, static_cast<uint32>(IndexCount), MaterialIndex });
			}
			if (Primitive.targets_count > 0)
			{
				Result.Warnings.push_back("Morph Target은 Static Mesh base pose에서 제외했다. Count=" + std::to_string(Primitive.targets_count));
			}
		}
		return true;
	};

	TArray<FMeshBuildData> Meshes;
	if (ImportOptions.bCombineMeshes)
	{
		FMeshBuildData& Combined = Meshes.emplace_back();
		Combined.Name = Sanitize(nullptr, FPaths::ToUtf8(SourceFilePath.stem().wstring()));
		const auto AppendNode = [&](const auto& Self, const cgltf_node& Node) -> bool
		{
			if (Node.mesh)
			{
				float GLTFWorldMatrix[16];
				cgltf_node_transform_world(&Node, GLTFWorldMatrix);
				const FMatrix WorldMatrix = ConvertMatrix(GLTFWorldMatrix);
				if (!AppendMesh(*Node.mesh, &WorldMatrix, Combined))
				{
					return false;
				}
			}
			for (cgltf_size ChildIndex = 0; ChildIndex < Node.children_count; ++ChildIndex)
			{
				if (!Self(Self, *Node.children[ChildIndex]))
				{
					return false;
				}
			}
			return true;
		};
		const cgltf_scene* Scene = GLTF.scene ? GLTF.scene : GLTF.scenes_count > 0 ? &GLTF.scenes[0] : nullptr;
		if (Scene)
		{
			for (cgltf_size RootIndex = 0; RootIndex < Scene->nodes_count; ++RootIndex)
			{
				if (!AppendNode(AppendNode, *Scene->nodes[RootIndex]))
				{
					return Result;
				}
			}
		}
		else
		{
			for (cgltf_size NodeIndex = 0; NodeIndex < GLTF.nodes_count; ++NodeIndex)
			{
				const cgltf_node& Node = GLTF.nodes[NodeIndex];
				if (!Node.parent && !AppendNode(AppendNode, Node))
				{
					return Result;
				}
			}
		}
	}
	else
	{
		Meshes.reserve(GLTF.meshes_count);
		for (cgltf_size MeshIndex = 0; MeshIndex < GLTF.meshes_count; ++MeshIndex)
		{
			const cgltf_mesh& SourceMesh = GLTF.meshes[MeshIndex];
			FMeshBuildData& Mesh = Meshes.emplace_back();
			Mesh.Name = Sanitize(SourceMesh.name, FPaths::ToUtf8(SourceFilePath.stem().wstring()));
			if (!AppendMesh(SourceMesh, nullptr, Mesh))
			{
				return Result;
			}
		}
	}

	// 반투명 Material만 삼각형 순서를 보존하며 사용하지 않는 Material은 다른 Section에 영향을 주지 않는다.
	TArray<uint8> ReorderTrianglesByMaterial(GLTF.materials_count, 1);
	for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.materials_count; ++MaterialIndex)
	{
		ReorderTrianglesByMaterial[MaterialIndex] = GLTF.materials[MaterialIndex].alpha_mode != cgltf_alpha_mode_blend;
	}

	for (FMeshBuildData& Mesh : Meshes)
	{
		if (Mesh.Vertices.empty())
		{
			Result.Error = "GLB에 Import할 Mesh Node가 없다.";
			return Result;
		}
		if (bEngineGeometry)
		{
			Normalize(Mesh.Vertices, ImportOptions.UniformScale);
		}
		GenerateTangents(Mesh.Vertices, Mesh.Indices);
		
		const auto SimplificationStart = std::chrono::steady_clock::now();
		TArray<FStaticMeshLODBuildData> GeneratedLODs = FMeshSimplifier::GenerateLODs(
			{ Mesh.Vertices, Mesh.Indices, Mesh.Sections, ImportOptions.LODTriangleRatios });
		const double SimplificationMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - SimplificationStart).count();
		
		KE_LOG(LogAssetImporter, Display, "LOD 간소화 완료. Mesh={}, SourceVertices={}, SourceTriangles={}, LODs={}, CPU={}ms",
			Mesh.Name, Mesh.Vertices.size(), Mesh.Indices.size() / 3, GeneratedLODs.size(), SimplificationMilliseconds);

		if (GeneratedLODs.size() < ImportOptions.LODTriangleRatios.size())
		{
			Result.Warnings.push_back("LOD 생성이 경계 또는 삼각형 뒤집힘 검사로 조기 종료됐다: " + Mesh.Name);
		}

		for (FStaticMeshLODBuildData& LOD : GeneratedLODs)
		{
			const auto OptimizationStart = std::chrono::steady_clock::now();
			GenerateTangents(LOD.Vertices, LOD.Indices);
			const FMeshOptimizationStatistics Statistics = FMeshOptimizer::Optimize(LOD.Vertices, LOD.Indices, LOD.Sections, ReorderTrianglesByMaterial);
			const double OptimizationMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - OptimizationStart).count();
			KE_LOG(LogAssetImporter, Display, "LOD 통계. Mesh={}, Ratio={}, Triangles={}, Vertices={}, Indices={}, FIFO={}/{}/{}, CPU={}ms",
				Mesh.Name, LOD.AchievedTriangleRatio, LOD.Indices.size() / 3, LOD.Vertices.size(), LOD.Indices.size(),
				Statistics.CacheMissesBefore, Statistics.CacheMissesAfterCache, Statistics.CacheMissesAfterOverdraw, OptimizationMilliseconds);
		}

		for (SIZE_T LODIndex = 0; LODIndex < GeneratedLODs.size(); ++LODIndex)
		{
			if (GeneratedLODs[LODIndex].AchievedTriangleRatio > ImportOptions.LODTriangleRatios[LODIndex] + 0.005f)
			{
				Result.Warnings.push_back("LOD 목표 삼각형 비율에 도달하지 못했다: " + Mesh.Name + " LOD " + std::to_string(LODIndex + 1));
			}
		}

		FMeshOptimizer::Optimize(Mesh.Vertices, Mesh.Indices, Mesh.Sections, ReorderTrianglesByMaterial);

		TArray<uint8> PayloadBytes;
		FMemoryWriter Payload(PayloadBytes);
		FStaticMeshPayloadHeader Header = {};
		Header.VertexStride = sizeof(FStaticMeshVertex);
		Header.LODCount = 1 + static_cast<uint32>(GeneratedLODs.size());
		Header.MaterialCount = MaterialAssetIds.empty() ? 1 : static_cast<uint32>(MaterialAssetIds.size());
		Payload << Header;
		if (MaterialAssetIds.empty())
		{
			FString SlotName = "Default";
			FAssetId MaterialId = bEngineGeometry ? FEngineAssetIds::DefaultWhiteMaterial : FAssetId();
			Payload << SlotName;
			Payload << MaterialId;
		}
		for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.materials_count; ++MaterialIndex)
		{
			FString SlotName = Sanitize(GLTF.materials[MaterialIndex].name, "Material_" + std::to_string(MaterialIndex));
			FAssetId MaterialId = MaterialAssetIds[MaterialIndex];
			Payload << SlotName;
			Payload << MaterialId;
		}
		const auto SerializeLOD = [&Payload](TArray<FStaticMeshVertex>& Vertices, TArray<uint32>& Indices, TArray<FStaticMeshSection>& Sections)
		{
			FStaticMeshLODPayloadHeader LODHeader = {
				static_cast<uint32>(Vertices.size()), static_cast<uint32>(Indices.size()), static_cast<uint32>(Sections.size())
			};
			Payload << LODHeader;
			Payload.Serialize(Vertices.data(), static_cast<int64>(Vertices.size() * sizeof(FStaticMeshVertex)));
			Payload.Serialize(Indices.data(), static_cast<int64>(Indices.size() * sizeof(uint32)));
			Payload.Serialize(Sections.data(), static_cast<int64>(Sections.size() * sizeof(FStaticMeshSection)));
		};
		SerializeLOD(Mesh.Vertices, Mesh.Indices, Mesh.Sections);
		for (FStaticMeshLODBuildData& LOD : GeneratedLODs)
		{
			SerializeLOD(LOD.Vertices, LOD.Indices, LOD.Sections);
		}
		const FString AssetPath = Combine(MeshRoot, Mesh.Name);
		FAssetId StaticMeshId;
		if (Payload.HasError() || !SaveAsset(AssetPath, EAssetType::StaticMesh, FStaticMeshPayloadHeader::CurrentVersion, PayloadBytes, StaticMeshId))
		{
			Result.Error = "Static Mesh .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::StaticMesh, StaticMeshId });
	}

	if (ImportOptions.bCombineMeshes)
	{
		const FString CombinedAssetPath = Combine(MeshRoot, Meshes.front().Name);
		for (cgltf_size MeshIndex = 0; MeshIndex < GLTF.meshes_count; ++MeshIndex)
		{
			const FString OldMeshName = Sanitize(GLTF.meshes[MeshIndex].name, FPaths::ToUtf8(SourceFilePath.stem().wstring()));
			const FString OldAssetPath = Combine(MeshRoot, OldMeshName);
			if (OldAssetPath != CombinedAssetPath)
			{
				std::error_code RemoveError;
				std::filesystem::remove(FPaths::ResolveContentPath(OldAssetPath + ".kasset"), RemoveError);
				if (RemoveError)
				{
					KE_LOG(LogAssetImporter, Warning, "이전 개별 Static Mesh를 제거하지 못했다. AssetPath={}, Error={}",
					       OldAssetPath, RemoveError.message());
				}
			}
		}
	}
	else
	{
		const FString CombinedName = Sanitize(nullptr, FPaths::ToUtf8(SourceFilePath.stem().wstring()));
		const bool bCombinedPathReused = std::ranges::any_of(Meshes, [&CombinedName](const FMeshBuildData& Mesh)
		                                                     { return Mesh.Name == CombinedName; });
		if (!bCombinedPathReused)
		{
			std::error_code RemoveError;
			const FString CombinedAssetPath = Combine(MeshRoot, CombinedName);
			std::filesystem::remove(FPaths::ResolveContentPath(CombinedAssetPath + ".kasset"), RemoveError);
			if (RemoveError)
			{
				KE_LOG(LogAssetImporter, Warning, "이전 결합 Static Mesh를 제거하지 못했다. AssetPath={}, Error={}",
				       CombinedAssetPath, RemoveError.message());
			}
		}
	}
	Result.bSucceeded = true;
	return Result;
}

// 단일 GLB를 읽어 Mesh, Material, Texture .kasset으로 변환한다.
FAssetImportResult FAssetImporter::ImportGLB(
    const std::filesystem::path& SourceFilePath,
    const FString& DestinationAssetPath,
    const FGLBImportOptions& ImportOptions) const
{
	FAssetImportResult Result;
	if (!std::isfinite(ImportOptions.UniformScale) || ImportOptions.UniformScale <= 0.0f)
	{
		Result.Error = "Uniform Scale은 0보다 큰 유한한 값이어야 한다.";
		return Result;
	}

	float PreviousRatio = 1.0f;
	for (const float Ratio : ImportOptions.LODTriangleRatios)
	{
		if (!std::isfinite(Ratio) || Ratio <= 0.0f || Ratio >= PreviousRatio)
		{
			Result.Error = "LOD 삼각형 비율은 0보다 크고 이전 LOD보다 작아야 한다.";
			return Result;
		}
		PreviousRatio = Ratio;
	}

	const FString SourcePathUtf8 = FPaths::ToUtf8(SourceFilePath.wstring());
	cgltf_options GLTFOptions = {};
	FGLTFGuard GLTF;
	if (cgltf_parse_file(&GLTFOptions, SourcePathUtf8.c_str(), &GLTF.Data) != cgltf_result_success)
	{
		Result.Error = "GLB 파싱에 실패했다: " + SourcePathUtf8;
		return Result;
	}
	const bool bHasTextures = GLTF.Data->images_count > 0;
	const bool bHasMaterials = GLTF.Data->materials_count > 0;
	const bool bEngineGeometry = DestinationAssetPath.starts_with("/Engine/Model/");
	const FString TextureRoot = bHasTextures ? Combine(DestinationAssetPath, "Texture") : DestinationAssetPath;
	const FString MaterialRoot = bHasMaterials ? Combine(DestinationAssetPath, "Material") : DestinationAssetPath;
	const FString MeshRoot = bHasTextures || bHasMaterials ? Combine(DestinationAssetPath, "Mesh") : DestinationAssetPath;

	if (cgltf_load_buffers(&GLTFOptions, GLTF.Data, SourcePathUtf8.c_str()) != cgltf_result_success ||
	    cgltf_validate(GLTF.Data) != cgltf_result_success)
	{
		Result.Error = "GLB Buffer 로드 또는 검증에 실패했다: " + SourcePathUtf8;
		return Result;
	}
	if (GLTF.Data->skins_count > 0 || GLTF.Data->animations_count > 0)
	{
		Result.Error = "Static Mesh Import는 Skin과 Animation을 지원하지 않는다.";
		return Result;
	}

	const auto AppendResult = [&Result](FAssetImportResult&& ImportResult)
	{
		const bool bSucceeded = ImportResult.bSucceeded;
		for (FImportedAsset& Asset : ImportResult.ImportedAssets)
		{
			Result.ImportedAssets.push_back(std::move(Asset));
		}
		for (FString& Warning : ImportResult.Warnings)
		{
			Result.Warnings.push_back(std::move(Warning));
		}
		if (!bSucceeded)
		{
			Result.Error = std::move(ImportResult.Error);
		}
		return bSucceeded;
	};

	FAssetImportResult TextureResult = ImportTextures({ *GLTF.Data, TextureRoot });
	TArray<FAssetId> TextureAssetIds;
	TextureAssetIds.reserve(TextureResult.ImportedAssets.size());
	for (const FImportedAsset& Asset : TextureResult.ImportedAssets)
	{
		TextureAssetIds.push_back(Asset.AssetId);
	}
	if (!AppendResult(std::move(TextureResult)))
	{
		return Result;
	}

	FAssetImportResult MaterialResult = ImportMaterials({ *GLTF.Data, MaterialRoot, TextureAssetIds });
	TArray<FAssetId> MaterialAssetIds;
	MaterialAssetIds.reserve(MaterialResult.ImportedAssets.size());
	for (const FImportedAsset& Asset : MaterialResult.ImportedAssets)
	{
		MaterialAssetIds.push_back(Asset.AssetId);
	}
	if (!AppendResult(std::move(MaterialResult)))
	{
		return Result;
	}

	FAssetImportResult StaticMeshResult = ImportStaticMeshes({ bEngineGeometry, *GLTF.Data, SourceFilePath, MeshRoot, ImportOptions, MaterialAssetIds });
	if (!AppendResult(std::move(StaticMeshResult)))
	{
		return Result;
	}

	Result.bSucceeded = true;
	return Result;
}
