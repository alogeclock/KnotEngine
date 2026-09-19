#pragma once

#include "EngineAPI.h"

#include "Core/Math/Vector.h"

struct FMatrix;

struct ENGINE_API FRay
{
	// 멤버 변수 (Member Variables)
	FVector Origin;
	FVector Direction;
	FVector InvD; // 1/Direction, 평행 케이스는 IEEE 754 ±infinity로 자동 처리

	// 생성자 (Constructors)
	constexpr FRay()
	    : Origin(), Direction(), InvD() {}
	FRay(const FVector& InOrigin, const FVector& InDirection);

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	void SetDirection(const FVector& NewDirection);
	bool Intersect(const FVector& Vertex0, const FVector& Vertex1, const FVector& Vertex2, float& OutDistance) const noexcept;

	// 공용 기하 계산기 (Static Geometry Functions)
	static FRay BuildRay(float MouseX, float MouseY, const FMatrix& ViewProjection, float ViewportWidth, float ViewportHeight);
};
