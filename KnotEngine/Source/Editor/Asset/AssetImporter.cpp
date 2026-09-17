#include "Asset/AssetImporter.h"

#include "Asset/Material/Material.h"
#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Texture/Texture.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/MemoryArchive.h"

#include <DirectXTex.h>
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

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
bool FAssetImporter::SaveAsset(const FString& AssetPath, EAssetType Type, uint32 PayloadVersion, const TArray<uint8>& PayloadBytes)
{
	const std::filesystem::path FinalPath = FPaths::ResolveContentPath(AssetPath + ".kasset");
	const std::filesystem::path TemporaryPath = FinalPath.wstring() + L".tmp";
	std::error_code Error;
	std::filesystem::create_directories(FinalPath.parent_path(), Error);
	if (Error)
	{
		return false;
	}

	FAssetFileHeader Header = {};
	std::memcpy(Header.Magic, FAssetFileHeader::MagicValue, sizeof(Header.Magic));
	Header.ContainerVersion = FAssetFileHeader::CurrentVersion;
	Header.AssetType = Type;
	Header.PayloadVersion = PayloadVersion;
	Header.PayloadSize = PayloadBytes.size();
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
	return !Error;
}

// 생성된 .kasset의 수정 시각과 Header가 현재 Source 및 Asset 형식과 일치하는지 확인한다.
bool FAssetImporter::IsAssetUpToDate(
	const FString& AssetPath,
	const std::filesystem::file_time_type& SourceTimestamp,
	EAssetType ExpectedType,
	uint32 ExpectedPayloadVersion)
{
	std::error_code Error;
	const std::filesystem::path FilePath = FPaths::ResolveContentPath(AssetPath + ".kasset");
	if (!std::filesystem::exists(FilePath, Error) || Error || std::filesystem::last_write_time(FilePath, Error) < SourceTimestamp || Error)
	{
		return false;
	}

	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	std::streamoff FileSize = -1;
	if (Stream)
	{
		FileSize = Stream.tellg();
	}
	TArray<uint8> HeaderBytes(sizeof(FAssetFileHeader));
	if (FileSize < static_cast<std::streamoff>(HeaderBytes.size()))
	{
		return false;
	}
	Stream.seekg(0, std::ios::beg);
	if (!Stream.read(reinterpret_cast<char*>(HeaderBytes.data()), static_cast<std::streamsize>(HeaderBytes.size())))
	{
		return false;
	}

	FMemoryReader HeaderArchive(HeaderBytes);
	FAssetFileHeader Header = {};
	HeaderArchive << Header;
	return !HeaderArchive.HasError() && !HeaderArchive.CanSerialize(1) &&
		std::memcmp(Header.Magic, FAssetFileHeader::MagicValue, sizeof(Header.Magic)) == 0 &&
		Header.ContainerVersion == FAssetFileHeader::CurrentVersion && Header.AssetType == ExpectedType &&
		Header.PayloadVersion == ExpectedPayloadVersion && Header.PayloadSize == static_cast<uint64>(FileSize) - HeaderBytes.size();
}

// glTF의 오른손 Y-Up 미터 좌표를 Knot Engine의 왼손 Z-Up 센티미터 좌표로 변환한다.
FVector FAssetImporter::ConvertPosition(const float Value[3])
{
	return FVector(-Value[2] * 100.0f, Value[0] * 100.0f, Value[1] * 100.0f);
}

// glTF 방향 벡터를 Knot Engine 좌표계로 변환하고 정규화한다.
FVector FAssetImporter::ConvertDirection(const float Value[3])
{
	return FVector(-Value[2], Value[0], Value[1]).GetSafeNormal();
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
		? ESamplerFilter::Point : Sampler->min_filter == cgltf_filter_type_linear_mipmap_linear ? ESamplerFilter::Trilinear : ESamplerFilter::Bilinear;
	const auto ConvertAddress = [](cgltf_int Value)
	{
		if (Value == 33071) return ESamplerAddressMode::Clamp;
		if (Value == 33648) return ESamplerAddressMode::Mirror;
		return ESamplerAddressMode::Wrap;
	};
	Result.AddressU = ConvertAddress(Sampler->wrap_s);
	Result.AddressV = ConvertAddress(Sampler->wrap_t);
	return Result;
}

// 단일 GLB를 읽어 Mesh, Material, Texture .kasset으로 변환한다.
FAssetImportResult FAssetImporter::ImportGLB(
	const std::filesystem::path& SourceFilePath,
	const FString& DestinationAssetPath,
	bool bCreateTypeFolders) const
{
	FAssetImportResult Result;
	const FString SourcePathUtf8 = FPaths::ToUtf8(SourceFilePath.wstring());
	const FString TextureRoot = bCreateTypeFolders ? Combine(DestinationAssetPath, "Texture") : DestinationAssetPath;
	const FString MaterialRoot = bCreateTypeFolders ? Combine(DestinationAssetPath, "Material") : DestinationAssetPath;
	const FString MeshRoot = bCreateTypeFolders ? Combine(DestinationAssetPath, "Mesh") : DestinationAssetPath;
	cgltf_options Options = {};
	FGLTFGuard GLTF;
	if (cgltf_parse_file(&Options, SourcePathUtf8.c_str(), &GLTF.Data) != cgltf_result_success)
	{
		Result.Error = "GLB 파싱에 실패했다: " + SourcePathUtf8;
		return Result;
	}

	std::error_code TimestampError;
	const auto SourceTimestamp = std::filesystem::last_write_time(SourceFilePath, TimestampError);
	bool bCurrent = !TimestampError && GLTF.Data->meshes_count > 0;
	for (cgltf_size ImageIndex = 0; ImageIndex < GLTF.Data->images_count; ++ImageIndex)
	{
		bCurrent = bCurrent && IsAssetUpToDate(
			Combine(TextureRoot, Sanitize(GLTF.Data->images[ImageIndex].name, "Texture_" + std::to_string(ImageIndex))),
			SourceTimestamp,
			EAssetType::Texture2D,
			FTexture2DPayloadHeader::CurrentVersion);
	}
	for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.Data->materials_count; ++MaterialIndex)
	{
		bCurrent = bCurrent && IsAssetUpToDate(
			Combine(MaterialRoot, Sanitize(GLTF.Data->materials[MaterialIndex].name, "Material_" + std::to_string(MaterialIndex))),
			SourceTimestamp,
			EAssetType::Material,
			FMaterialPayloadHeader::CurrentVersion);
	}
	for (cgltf_size MeshIndex = 0; MeshIndex < GLTF.Data->meshes_count; ++MeshIndex)
	{
		bCurrent = bCurrent && IsAssetUpToDate(
			Combine(MeshRoot, Sanitize(GLTF.Data->meshes[MeshIndex].name, FPaths::ToUtf8(SourceFilePath.stem().wstring()))),
			SourceTimestamp,
			EAssetType::StaticMesh,
			FStaticMeshPayloadHeader::CurrentVersion);
	}
	if (bCurrent)
	{
		Result.bSucceeded = true;
		return Result;
	}
	if (cgltf_load_buffers(&Options, GLTF.Data, SourcePathUtf8.c_str()) != cgltf_result_success || cgltf_validate(GLTF.Data) != cgltf_result_success)
	{
		Result.Error = "GLB Buffer 로드 또는 검증에 실패했다: " + SourcePathUtf8;
		return Result;
	}
	if (GLTF.Data->skins_count > 0 || GLTF.Data->animations_count > 0)
	{
		Result.Error = "Static Mesh Import는 Skin과 Animation을 지원하지 않는다.";
		return Result;
	}

	TArray<FString> ImageAssetPaths(GLTF.Data->images_count);
	for (cgltf_size ImageIndex = 0; ImageIndex < GLTF.Data->images_count; ++ImageIndex)
	{
		const cgltf_image& Image = GLTF.Data->images[ImageIndex];
		if (!Image.buffer_view || !Image.buffer_view->buffer || !Image.buffer_view->buffer->data)
		{
			Result.Error = "외부 URI Image는 현재 지원하지 않는다.";
			return Result;
		}
		const uint8* ImageBytes = static_cast<const uint8*>(Image.buffer_view->buffer->data) + Image.buffer_view->offset;
		DirectX::ScratchImage Decoded;
		DirectX::ScratchImage MipChain;
		DirectX::ScratchImage Compressed;
		DirectX::TexMetadata Metadata;
		if (FAILED(DirectX::LoadFromWICMemory(ImageBytes, Image.buffer_view->size, DirectX::WIC_FLAGS_FORCE_RGB, &Metadata, Decoded)) ||
			FAILED(DirectX::GenerateMipMaps(Decoded.GetImages(), Decoded.GetImageCount(), Decoded.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, MipChain)) ||
			FAILED(DirectX::Compress(MipChain.GetImages(), MipChain.GetImageCount(), MipChain.GetMetadata(), DXGI_FORMAT_BC7_UNORM,
				DirectX::TEX_COMPRESS_PARALLEL | DirectX::TEX_COMPRESS_BC7_QUICK, DirectX::TEX_THRESHOLD_DEFAULT, Compressed)))
		{
			Result.Error = "Texture decode, mip 생성 또는 BC7 압축에 실패했다.";
			return Result;
		}

		const FString TextureName = Sanitize(Image.name, "Texture_" + std::to_string(ImageIndex));
		const FString AssetPath = Combine(TextureRoot, TextureName);
		TArray<uint8> PayloadBytes;
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
		if (Payload.HasError() || !SaveAsset(AssetPath, EAssetType::Texture2D, FTexture2DPayloadHeader::CurrentVersion, PayloadBytes))
		{
			Result.Error = "Texture .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		ImageAssetPaths[ImageIndex] = AssetPath;
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::Texture2D });
	}

	TArray<FString> MaterialAssetPaths(GLTF.Data->materials_count);
	for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.Data->materials_count; ++MaterialIndex)
	{
		const cgltf_material& Source = GLTF.Data->materials[MaterialIndex];
		const bool bMasked = Source.alpha_mode == cgltf_alpha_mode_mask;
		const bool bTranslucent = Source.alpha_mode == cgltf_alpha_mode_blend;
		const FString MaterialName = Sanitize(Source.name, "Material_" + std::to_string(MaterialIndex));
		const FString AssetPath = Combine(MaterialRoot, MaterialName);
		FMaterialPayloadHeader Header = {};
		Header.BlendMode = bTranslucent ? EMaterialBlendMode::Translucent : bMasked ? EMaterialBlendMode::Masked : EMaterialBlendMode::Opaque;
		Header.DepthMode = EMaterialDepthMode::ReadWrite;
		Header.CullMode = Source.double_sided ? ECullMode::None : ECullMode::Back;
		Header.ScalarParameterCount = 1;
		Header.VectorParameterCount = 1;
		Header.TextureParameterCount = Source.has_pbr_metallic_roughness && Source.pbr_metallic_roughness.base_color_texture.texture ? 1 : 0;

		TArray<uint8> PayloadBytes;
		FMemoryWriter Payload(PayloadBytes);
		Payload << Header;
		FShaderKey VertexShader = { "/Engine/Shader/StaticMesh.hlsl", "MainVS", EShaderStage::Vertex };
		FShaderKey PixelShader = {
			"/Engine/Shader/StaticMesh.hlsl", bTranslucent ? "TranslucentPS" : bMasked ? "MaskedPS" : "OpaquePS", EShaderStage::Pixel
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
			const SIZE_T ImageIndex = static_cast<SIZE_T>(View.texture->image - GLTF.Data->images);
			FString BaseColorTextureName = "BaseColorTexture";
			FString TexturePath = ImageAssetPaths[ImageIndex];
			FSamplerDesc Sampler = ConvertSampler(View.texture->sampler);
			Payload << BaseColorTextureName;
			Payload << TexturePath;
			Payload << Sampler;
		}
		if (Payload.HasError() || !SaveAsset(AssetPath, EAssetType::Material, FMaterialPayloadHeader::CurrentVersion, PayloadBytes))
		{
			Result.Error = "Material .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		MaterialAssetPaths[MaterialIndex] = AssetPath;
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::Material });
	}

	for (cgltf_size MeshIndex = 0; MeshIndex < GLTF.Data->meshes_count; ++MeshIndex)
	{
		const cgltf_mesh& SourceMesh = GLTF.Data->meshes[MeshIndex];
		TArray<FStaticMeshVertex> Vertices;
		TArray<uint32> Indices;
		TArray<FStaticMeshSection> Sections;
		for (cgltf_size PrimitiveIndex = 0; PrimitiveIndex < SourceMesh.primitives_count; ++PrimitiveIndex)
		{
			const cgltf_primitive& Primitive = SourceMesh.primitives[PrimitiveIndex];
			if (Primitive.type != cgltf_primitive_type_triangles)
			{
				Result.Error = "Triangle이 아닌 GLB Primitive는 지원하지 않는다.";
				return Result;
			}
			const cgltf_accessor* Positions = FindAttribute(Primitive, cgltf_attribute_type_position);
			const cgltf_accessor* Normals = FindAttribute(Primitive, cgltf_attribute_type_normal);
			const cgltf_accessor* TexCoords = FindAttribute(Primitive, cgltf_attribute_type_texcoord, 0);
			if (!Positions || Positions->count == 0)
			{
				Result.Error = "POSITION이 없는 GLB Primitive다.";
				return Result;
			}
			const uint32 BaseVertex = static_cast<uint32>(Vertices.size());
			for (cgltf_size VertexIndex = 0; VertexIndex < Positions->count; ++VertexIndex)
			{
				float Position[3] = {};
				float Normal[3] = { 0.0f, 1.0f, 0.0f };
				float UV[2] = {};
				cgltf_accessor_read_float(Positions, VertexIndex, Position, 3);
				if (Normals) cgltf_accessor_read_float(Normals, VertexIndex, Normal, 3);
				if (TexCoords) cgltf_accessor_read_float(TexCoords, VertexIndex, UV, 2);
				FStaticMeshVertex Vertex;
				Vertex.Position = ConvertPosition(Position);
				Vertex.Normal = ConvertDirection(Normal);
				Vertex.TexCoord = FVector2(UV[0], UV[1]);
				Vertices.push_back(Vertex);
			}

			const uint32 FirstIndex = static_cast<uint32>(Indices.size());
			const cgltf_size IndexCount = Primitive.indices ? Primitive.indices->count : Positions->count;
			if (IndexCount % 3 != 0)
			{
				Result.Error = "Triangle Primitive의 Index 개수가 3의 배수가 아니다.";
				return Result;
			}
			for (cgltf_size Index = 0; Index < IndexCount; Index += 3)
			{
				const auto ReadIndex = [&Primitive](cgltf_size Value)
				{
					return Primitive.indices ? cgltf_accessor_read_index(Primitive.indices, Value) : Value;
				};
				Indices.push_back(BaseVertex + static_cast<uint32>(ReadIndex(Index)));
				Indices.push_back(BaseVertex + static_cast<uint32>(ReadIndex(Index + 2)));
				Indices.push_back(BaseVertex + static_cast<uint32>(ReadIndex(Index + 1)));
			}
			const uint32 MaterialIndex = Primitive.material ? static_cast<uint32>(Primitive.material - GLTF.Data->materials) : 0;
			Sections.push_back({ FirstIndex, static_cast<uint32>(IndexCount), MaterialIndex });
			if (Primitive.targets_count > 0)
			{
				Result.Warnings.push_back("Morph Target은 Static Mesh base pose에서 제외했다. Count=" + std::to_string(Primitive.targets_count));
			}
		}
		if (DestinationAssetPath.starts_with("/Engine/Model/"))
		{
			Normalize(Vertices, 100.0f); // 기본 Geometry의 가장 긴 축을 1m에 맞춘다.
		}
		GenerateTangents(Vertices, Indices);

		TArray<uint8> PayloadBytes;
		FMemoryWriter Payload(PayloadBytes);
		FStaticMeshPayloadHeader Header = {};
		Header.VertexStride = sizeof(FStaticMeshVertex);
		Header.LODCount = 1;
		Header.MaterialCount = MaterialAssetPaths.empty() ? 1 : static_cast<uint32>(MaterialAssetPaths.size());
		Payload << Header;
		if (MaterialAssetPaths.empty())
		{
			FString SlotName = "Default";
			FString MaterialPath;
			Payload << SlotName;
			Payload << MaterialPath;
		}
		for (cgltf_size MaterialIndex = 0; MaterialIndex < GLTF.Data->materials_count; ++MaterialIndex)
		{
			FString SlotName = Sanitize(GLTF.Data->materials[MaterialIndex].name, "Material_" + std::to_string(MaterialIndex));
			FString MaterialPath = MaterialAssetPaths[MaterialIndex];
			Payload << SlotName;
			Payload << MaterialPath;
		}
		FStaticMeshLODPayloadHeader LODHeader = {
			static_cast<uint32>(Vertices.size()), static_cast<uint32>(Indices.size()), static_cast<uint32>(Sections.size())
		};
		Payload << LODHeader;
		Payload.Serialize(Vertices.data(), static_cast<int64>(Vertices.size() * sizeof(FStaticMeshVertex)));
		Payload.Serialize(Indices.data(), static_cast<int64>(Indices.size() * sizeof(uint32)));
		Payload.Serialize(Sections.data(), static_cast<int64>(Sections.size() * sizeof(FStaticMeshSection)));
		const FString MeshName = Sanitize(SourceMesh.name, FPaths::ToUtf8(SourceFilePath.stem().wstring()));
		const FString AssetPath = Combine(MeshRoot, MeshName);
		if (Payload.HasError() || !SaveAsset(AssetPath, EAssetType::StaticMesh, FStaticMeshPayloadHeader::CurrentVersion, PayloadBytes))
		{
			Result.Error = "Static Mesh .kasset 저장에 실패했다: " + AssetPath;
			return Result;
		}
		Result.ImportedAssets.push_back({ AssetPath, EAssetType::StaticMesh });
	}

	Result.bSucceeded = true;
	return Result;
}

// Content 아래의 모든 GLB를 탐색하여 변경된 Source Asset을 Import한다.
bool FAssetImporter::ImportAllGLB() const
{
	const std::filesystem::path ContentRoot(FPaths::ContentDir());
	std::error_code Error;
	for (std::filesystem::recursive_directory_iterator It(ContentRoot, std::filesystem::directory_options::skip_permission_denied, Error), End;
		It != End; It.increment(Error))
	{
		if (Error)
		{
			return false;
		}
		if (!It->is_regular_file() || It->path().extension() != L".glb")
		{
			continue;
		}
		const std::filesystem::path RelativeParent = std::filesystem::relative(It->path().parent_path(), ContentRoot);
		const FString Destination = "/" + FPaths::ToUtf8(RelativeParent.generic_wstring());
		const bool bEnginePrimitive = Destination.starts_with("/Engine/Model/");
		const FAssetImportResult Result = ImportGLB(It->path(), Destination, !bEnginePrimitive);
		if (!Result.bSucceeded)
		{
			KE_LOG(LogAssetImporter, Error, "GLB Import 실패. Source={}, Error={}", FPaths::ToUtf8(It->path().wstring()), Result.Error);
			return false;
		}
		for (const FString& Warning : Result.Warnings)
		{
			KE_LOG(LogAssetImporter, Warning, "{}: {}", FPaths::ToUtf8(It->path().filename().wstring()), Warning);
		}
	}
	return true;
}
