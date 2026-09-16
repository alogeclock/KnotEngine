#pragma once

// 외부 GLB Asset을 엔진의 .kasset으로 변환하는 Editor 전용 계층의 Skeleton이다.
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
