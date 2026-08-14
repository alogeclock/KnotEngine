#pragma once

#include "Core/CoreTypes.h"
#include "Core/Geometry/Plane.h"

struct FAABB;
struct FMatrix;

struct FFrustum
{
	// 멤버 타입 및 변수 (Member Types and Variables)
	FPlane Planes[6];

	enum class EFrustumIntersectResult : uint8
	{
		Outside,
		Intersect,
		Inside
	};

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	void UpdateFromCamera(const FMatrix& View, const FMatrix& Projection);
	void UpdateFromCamera(const FMatrix& ViewProjection);
	EFrustumIntersectResult Intersects(const FAABB& Box) const;
	bool Contains(const FVector& Point) const;
};
