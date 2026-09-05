#pragma once

#include "EngineAPI.h"

#include "Core/Math/Vector.h"
#include "Core/Math/Quat.h"

struct FAABB;
struct FMatrix;

struct ENGINE_API FOBB
{
	// 멤버 변수 (Member Variables)
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;

	// 생성자 (Constructors)
	FOBB();
	FOBB(const FVector& InCenter, const FVector& InExtent, const FQuat& InRotation);
	FOBB(const FVector& InCenter, const FVector& InExtent, const FMatrix& InMatrix);

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	void Reset();
	bool IsValid() const;
	bool IntersectOBBAABB(const FAABB& AABB) const;

	// 공용 기하 계산기 (Static Geometry Functions)
	static FOBB FromAABB(const FAABB& InAABB, const FMatrix& InTransform);
};
