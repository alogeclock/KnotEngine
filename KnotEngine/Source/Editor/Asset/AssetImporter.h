#pragma once

// 외부 원본 Asset을 엔진의 .kasset으로 변환하는 Editor 전용 계층의 Skeleton이다.
// 최초 구현에서는 Blender를 headless로 실행해 .blend의 Static Mesh를 변환한다.
class FAssetImporter final
{
public:
	FAssetImporter();
	~FAssetImporter();

	FAssetImporter(const FAssetImporter&) = delete;
	FAssetImporter& operator=(const FAssetImporter&) = delete;
	FAssetImporter(FAssetImporter&&) = delete;
	FAssetImporter& operator=(FAssetImporter&&) = delete;
};
