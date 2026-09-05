#pragma once

#include "EngineAPI.h"

#include "Object/Reflection/ReflectionMacros.h"

#include "Core/Math/Quat.h"
#include "Core/Math/Vector.h"

struct FMatrix;

USTRUCT()
struct ENGINE_API FTransform
{
	GENERATED_STRUCT(FTransform)

	// 멤버 변수 (Member Variables)
	static const FTransform Identity;

	UPROPERTY() FQuat Rotation = FQuat::Identity;
	UPROPERTY() FVector Translation = FVector::ZeroVector;
	UPROPERTY() FVector Scale3D = FVector::OneVector;

	// 생성자 (Constructors)
	FTransform() noexcept = default;
	FTransform(const FQuat& InRotation, const FVector& InTranslation = FVector::ZeroVector, const FVector& InScale3D = FVector::OneVector) noexcept;

	FTransform operator*(const FTransform& Other) const noexcept;
	FTransform& operator*=(const FTransform& Other) noexcept;

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	FVector TransformPosition(const FVector& Position) const noexcept;
	FVector TransformVector(const FVector& Vector) const noexcept;
	FVector InverseTransformPosition(const FVector& Position) const noexcept;
	FVector InverseTransformVector(const FVector& Vector) const noexcept;

	FMatrix ToMatrix() const noexcept;
	FTransform Inverse() const noexcept;

private:
	static FVector ComponentDivideSafe(const FVector& Numerator, const FVector& Denominator, float Tolerance = KMath::Epsilon) noexcept;
};
