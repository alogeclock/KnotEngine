#include "Asset/AssetBinaryLoader.h"

#include "Asset/Mesh/StaticMesh.h"
#include "Asset/Mesh/StaticMeshBinaryFormat.h"

#include "Core/IO/Paths.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

// 논리 Asset 경로에 대응하는 .kasset을 검증하고 역직렬화해 Static Mesh UObject를 생성한다.
UStaticMesh* FAssetBinaryLoader::LoadStaticMesh(const FString& AssetPath) const
{
	if (AssetPath.size() < 2 || AssetPath.front() != '/' || AssetPath.find("..") != FString::npos)
	{
		return nullptr;
	}

	const FString RelativePath = AssetPath.substr(1) + ".kasset";
	const std::filesystem::path FilePath = std::filesystem::path(FPaths::ContentDir()) / FPaths::ToWide(RelativePath);
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		return nullptr;
	}

	const std::streamoff FileSize = Stream.tellg();
	if (FileSize <= 0 || static_cast<uint64>(FileSize) > (std::numeric_limits<SIZE_T>::max)())
	{
		return nullptr;
	}
	TArray<uint8> FileBytes(static_cast<SIZE_T>(FileSize));
	Stream.seekg(0, std::ios::beg);
	if (!Stream.read(reinterpret_cast<char*>(FileBytes.data()), FileSize))
	{
		return nullptr;
	}

	SIZE_T Offset = 0;
	const auto ReadBytes = [&FileBytes, &Offset](void* Destination, SIZE_T Size)
	{
		if (Offset > FileBytes.size() || Size > FileBytes.size() - Offset)
		{
			return false;
		}
		std::memcpy(Destination, FileBytes.data() + Offset, Size);
		Offset += Size;
		return true;
	};

	FStaticMeshBinaryHeader Header;
	if (!ReadBytes(&Header, sizeof(Header)) || std::memcmp(Header.Magic, FStaticMeshBinaryHeader::MagicValue, sizeof(Header.Magic)) != 0 ||
		Header.Version != FStaticMeshBinaryHeader::CurrentVersion || Header.VertexStride != sizeof(FStaticMeshVertex) ||
		Header.LODCount == 0 || Header.LODCount > FStaticMeshBinaryHeader::MaxLODCount)
	{
		return nullptr;
	}

	FStaticMesh RenderData;
	for (uint32 LODIndex = 0; LODIndex < Header.LODCount; ++LODIndex)
	{
		FStaticMeshLODBinaryHeader LODHeader;
		if (!ReadBytes(&LODHeader, sizeof(LODHeader)) || LODHeader.VertexCount == 0)
		{
			return nullptr;
		}

		const SIZE_T RemainingBytes = FileBytes.size() - Offset;
		if (LODHeader.VertexCount > RemainingBytes / sizeof(FStaticMeshVertex))
		{
			return nullptr;
		}
		TArray<FStaticMeshVertex> Vertices(LODHeader.VertexCount);
		if (!ReadBytes(Vertices.data(), Vertices.size() * sizeof(FStaticMeshVertex)))
		{
			return nullptr;
		}

		const SIZE_T RemainingIndexBytes = FileBytes.size() - Offset;
		if (LODHeader.IndexCount > RemainingIndexBytes / sizeof(uint32))
		{
			return nullptr;
		}
		TArray<uint32> Indices(LODHeader.IndexCount);
		if (!Indices.empty() && !ReadBytes(Indices.data(), Indices.size() * sizeof(uint32)))
		{
			return nullptr;
		}
		if (!RenderData.AddLOD(Vertices, Indices))
		{
			return nullptr;
		}
	}

	if (Offset != FileBytes.size())
	{
		return nullptr;
	}

	UStaticMesh* Mesh = GUObjectManager.Create<UStaticMesh>();
	if (!Mesh->Initialize(AssetPath, std::move(RenderData)))
	{
		GUObjectManager.Destroy(Mesh);
		return nullptr;
	}
	return Mesh;
}
