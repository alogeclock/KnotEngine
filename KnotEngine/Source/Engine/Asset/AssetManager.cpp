#include "Asset/AssetManager.h"

#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Object/ReferenceCollector.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

FAssetManager* GAssetManager = nullptr;

FAssetManager::~FAssetManager()
{
	Release();
}

void FAssetManager::Create()
{
	checkf(!GAssetManager, "Asset Manager가 이미 생성되어 있다.");
	check(StaticMeshes.empty());
	GAssetManager = this;

	panicf(LoadStaticMesh("/Engine/Geometry/Cube"), "내장 Cube Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Sphere"), "내장 Sphere Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Quad"), "내장 Quad Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Cylinder"), "내장 Cylinder Static Mesh를 불러오지 못했다.");
	panicf(LoadStaticMesh("/Engine/Geometry/Capsule"), "내장 Capsule Static Mesh를 불러오지 못했다.");
}

void FAssetManager::Release()
{
	if (GAssetManager != this)
	{
		return;
	}

	for (const auto& Entry : StaticMeshes)
	{
		if (UStaticMesh* Mesh = Entry.second.Get())
		{
			GUObjectManager.Destroy(Mesh);
		}
	}
	StaticMeshes.clear();
	GAssetManager = nullptr;
}

// 캐시에 없으면 논리 Asset 경로에 대응하는 .kasset 파일을 읽어 Static Mesh를 등록한다.
// TODO: Binary Loader 계층을 별도 계층으로 분리한다.
UStaticMesh* FAssetManager::LoadStaticMesh(const FString& AssetPath)
{
	// 파일 전체와 각 LOD 앞에 저장되는 고정 크기 헤더를 정의한다.
	struct FStaticMeshFileHeader
	{
		char Magic[4];
		uint32 Version;
		uint32 VertexStride;
		uint32 LODCount;
	};

	struct FStaticMeshLODFileHeader
	{
		uint32 VertexCount;
		uint32 IndexCount;
	};
	static_assert(sizeof(FStaticMeshFileHeader) == 16);
	static_assert(sizeof(FStaticMeshLODFileHeader) == 8);

	static constexpr char Magic[4] = { 'K', 'M', 'S', 'H' };
	static constexpr uint32 StaticMeshVersion = 1;
	static constexpr uint32 MaxLODCount = 16;

	// 이미 등록된 Asset은 파일을 다시 읽지 않고 즉시 반환한다.
	check(GAssetManager == this);
	if (UStaticMesh* ExistingMesh = FindStaticMesh(AssetPath))
	{
		return ExistingMesh;
	}
	if (AssetPath.size() < 2 || AssetPath.front() != '/' || AssetPath.find("..") != FString::npos)
	{
		return nullptr;
	}

	// 논리 Asset 경로를 Contents 아래의 실제 .kasset 파일 경로로 변환한다.
	const FString RelativePath = AssetPath.substr(1) + ".kasset";
	const std::filesystem::path FilePath = std::filesystem::path(FPaths::ContentDir()) / FPaths::ToWide(RelativePath);
	std::ifstream Stream(FilePath, std::ios::binary | std::ios::ate);
	if (!Stream)
	{
		return nullptr;
	}

	// 파일 크기를 검증한 뒤 역직렬화에 사용할 메모리로 전체 내용을 읽는다.
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

	// 모든 구조체와 배열이 파일 범위를 넘지 않도록 Offset 기반으로 읽는다.
	SIZE_T Offset = 0;
	const auto ReadBytes = [&FileBytes, &Offset](void* Destination, SIZE_T Size)
	{
		if (Size > FileBytes.size() - Offset)
		{
			return false;
		}
		std::memcpy(Destination, FileBytes.data() + Offset, Size);
		Offset += Size;
		return true;
	};

	// 파일 형식과 Vertex Layout이 현재 엔진에서 처리할 수 있는지 검증한다.
	FStaticMeshFileHeader Header;
	if (!ReadBytes(&Header, sizeof(Header)) || std::memcmp(Header.Magic, Magic, sizeof(Magic)) != 0 || Header.Version != StaticMeshVersion ||
		Header.VertexStride != sizeof(FStaticMeshVertex) || Header.LODCount == 0 || Header.LODCount > MaxLODCount)
	{
		return nullptr;
	}

	// 각 LOD의 Vertex와 Index 데이터를 순서대로 복원해 Render Data를 구성한다.
	FStaticMesh RenderData;
	for (uint32 LODIndex = 0; LODIndex < Header.LODCount; ++LODIndex)
	{
		FStaticMeshLODFileHeader LODHeader;
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

	// 검증과 역직렬화가 끝난 Render Data를 Asset Manager에 등록한다.
	return RegisterStaticMesh(AssetPath, std::move(RenderData));
}

// 캐시에서 논리 Asset 경로에 대응하는 Static Mesh를 찾는다.
UStaticMesh* FAssetManager::FindStaticMesh(const FString& AssetPath) const
{
	const auto It = StaticMeshes.find(AssetPath);
	return It != StaticMeshes.end() ? It->second.Get() : nullptr;
}

// 완성된 Render Data를 UObject Static Mesh로 등록한다.
UStaticMesh* FAssetManager::RegisterStaticMesh(const FString& AssetPath, FStaticMesh&& RenderData)
{
	if (UStaticMesh* ExistingMesh = FindStaticMesh(AssetPath))
	{
		return ExistingMesh;
	}

	UStaticMesh* Mesh = GUObjectManager.Create<UStaticMesh>();
	if (!Mesh->Initialize(AssetPath, std::move(RenderData)))
	{
		GUObjectManager.Destroy(Mesh);
		return nullptr;
	}
	StaticMeshes.emplace(AssetPath, Mesh);
	return Mesh;
}

// FAssetManager는 UObject가 아니라 일반 C++ 객체이므로, 자동으로 수집되지 않는다.
void FAssetManager::AddReferencedObjects(FReferenceCollector& Collector) const
{
	for (const auto& Entry : StaticMeshes)
	{
		Collector.AddReferencedObject(Entry.second);
	}
}
