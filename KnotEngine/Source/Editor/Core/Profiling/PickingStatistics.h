#pragma once

#include "Core/CoreTypes.h"

// GT에서 마지막으로 완료된 Picking 질의와 실제 질의 횟수 기준의 누적 시간을 보관한다.
struct FPickingStatistics
{
	uint64 QueryCount = 0;
	uint64 BoundsTests = 0;
	uint64 CandidateMeshes = 0;
	uint64 TriangleTests = 0;
	double TotalTimeMs = 0.0;
	double SceneQueryTimeMs = 0.0;
	double MeshQueryTimeMs = 0.0;
	double AccumulatedTimeMs = 0.0;
	double MaxTimeMs = 0.0;
	bool bHit = false;
};
