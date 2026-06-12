#pragma once

#include <cassert>
#include <cmath>

#include "Core/CoreTypes.h"
#include "Core/Math/Utils.h"
#include "Core/Math/Vector.h"

struct FQuat;

/**
 * 회전(Rotation)을 나타내는 구조체.
 * Pitch(Y축 회전), Yaw(Z축 회전), Roll(X축 회전)을 사용합니다.
 */
struct FRotator
{
public:
	float Pitch = 0.0f;
	float Yaw   = 0.0f;
	float Roll  = 0.0f;

	static const FRotator ZeroRotator;

	// ──────────── Constructor ────────────
public:
	/** 기본 생성자 */
	constexpr FRotator() noexcept = default;

	/** 성분별 생성자 */
	constexpr FRotator(float InPitch, float InYaw, float InRoll) noexcept
		: Pitch(InPitch), Yaw(InYaw), Roll(InRoll)
	{
	}

	/** 쿼터니언으로부터 생성 (Quat.h에 구현됨) */
	explicit FRotator(const FQuat& InQuat) noexcept;

	// ──────────── Static Methods ────────────
public:
	/** 각도를 [0, 360) 범위로 감쌉니다. */
	static float ClampAxis(float AngleDegrees) noexcept
	{
		float ClampedAngle = std::fmod(AngleDegrees, 360.0f);
		if (ClampedAngle < 0.0f)
		{
			ClampedAngle += 360.0f;
		}
		return ClampedAngle;
	}

	/** 각도를 (-180, 180] 범위로 정규화합니다. */
	static float NormalizeAxis(float AngleDegrees) noexcept
	{
		float NormalizedAngle = ClampAxis(AngleDegrees);
		if (NormalizedAngle > 180.0f)
		{
			NormalizedAngle -= 360.0f;
		}
		return NormalizedAngle;
	}

	/** 엔진 오일러 각도 {Roll, Pitch, Yaw}로부터 로테이터를 생성합니다. */
	static FRotator MakeFromEuler(const FVector& InEulerDegrees) noexcept
	{
		return FRotator(InEulerDegrees.Y, InEulerDegrees.Z, InEulerDegrees.X);
	}

	// ──────────── Operators ────────────
public:
	/** 동등 비교 연산자 */
	bool operator==(const FRotator& Other) const noexcept
	{
		return Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;
	}

	/** 부등 비교 연산자 */
	bool operator!=(const FRotator& Other) const noexcept { return !(*this == Other); }

	/** 단항 마이너스 연산자 */
	FRotator operator-() const noexcept
	{
		return FRotator(-Pitch, -Yaw, -Roll);
	}

	/** 덧셈 연산자 */
	FRotator operator+(const FRotator& Other) const noexcept
	{
		return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
	}

	/** 뺄셈 연산자 */
	FRotator operator-(const FRotator& Other) const noexcept
	{
		return FRotator(Pitch - Other.Pitch, Yaw - Other.Yaw, Roll - Other.Roll);
	}

	/** 스칼라 곱셈 연산자 */
	FRotator operator*(float Scale) const noexcept
	{
		return FRotator(Pitch * Scale, Yaw * Scale, Roll * Scale);
	}

	/** 스칼라 나눗셈 연산자 */
	FRotator operator/(float Scale) const noexcept
	{
		assert(std::fabs(Scale) > KMath::Epsilon);
		return FRotator(Pitch / Scale, Yaw / Scale, Roll / Scale);
	}

	/** 복합 덧셈 연산자 */
	FRotator& operator+=(const FRotator& Other) noexcept
	{
		Pitch += Other.Pitch; Yaw += Other.Yaw; Roll += Other.Roll;
		return *this;
	}

	/** 복합 뺄셈 연산자 */
	FRotator& operator-=(const FRotator& Other) noexcept
	{
		Pitch -= Other.Pitch; Yaw -= Other.Yaw; Roll -= Other.Roll;
		return *this;
	}

	/** 복합 곱셈 연산자 */
	FRotator& operator*=(float Scale) noexcept
	{
		Pitch *= Scale; Yaw *= Scale; Roll *= Scale;
		return *this;
	}

	/** 복합 나눗셈 연산자 */
	FRotator& operator/=(float Scale) noexcept
	{
		assert(std::fabs(Scale) > KMath::Epsilon);
		Pitch /= Scale; Yaw /= Scale; Roll /= Scale;
		return *this;
	}

	// ──────────── Methods ────────────
public:
	/** 이 로테이터를 엔진 오일러 각도 {Roll, Pitch, Yaw}로 반환합니다. */
	FVector Euler() const noexcept { return FVector(Roll, Pitch, Yaw); }

	/** 이 로테이터가 바라보는 전방 벡터(+X)를 반환합니다. */
	FVector Vector() const noexcept
	{
		const float PitchRadians = KMath::ToRadian(Pitch);
		const float YawRadians   = KMath::ToRadian(Yaw);
		const float CosPitch     = std::cos(PitchRadians);
		return FVector(
			CosPitch * std::cos(YawRadians),
			CosPitch * std::sin(YawRadians),
			std::sin(PitchRadians)).GetSafeNormal();
	}

	/** 각 성분을 [0, 360) 범위로 보정합니다. */
	void Clamp() noexcept
	{
		Pitch = ClampAxis(Pitch);
		Yaw   = ClampAxis(Yaw);
		Roll  = ClampAxis(Roll);
	}

	/** 각 성분을 (-180, 180] 범위로 정규화합니다. */
	void Normalize() noexcept
	{
		Pitch = NormalizeAxis(Pitch);
		Yaw   = NormalizeAxis(Yaw);
		Roll  = NormalizeAxis(Roll);
	}

	/** 각 성분을 [0, 360) 범위로 보정한 복사본을 반환합니다. */
	FRotator GetDenormalized() const noexcept
	{
		FRotator Result = *this;
		Result.Clamp();
		return Result;
	}

	/** 각 성분을 (-180, 180] 범위로 정규화한 복사본을 반환합니다. */
	FRotator GetNormalized() const noexcept
	{
		FRotator Result = *this;
		Result.Normalize();
		return Result;
	}

	/** Pitch, Yaw, Roll에 델타 값을 더하고 결과를 반환합니다. */
	FRotator Add(float DeltaPitch, float DeltaYaw, float DeltaRoll) noexcept
	{
		Pitch += DeltaPitch; Yaw += DeltaYaw; Roll += DeltaRoll;
		return *this;
	}

	/** 성분 중 하나라도 NaN 또는 무한대이면 true를 반환합니다. */
	bool ContainsNaN() const noexcept
	{
		return !std::isfinite(Pitch) || !std::isfinite(Yaw) || !std::isfinite(Roll);
	}

	/** 정규화 기준으로 모든 각도가 정확히 0이면 true를 반환합니다. */
	bool IsZero() const noexcept
	{
		return NormalizeAxis(Pitch) == 0.0f
			&& NormalizeAxis(Yaw)   == 0.0f
			&& NormalizeAxis(Roll)  == 0.0f;
	}

	/** 허용 오차 내에서 다른 로테이터와 같은 회전인지 비교합니다. */
	bool Equals(const FRotator& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(NormalizeAxis(Pitch - Other.Pitch)) <= Tolerance
			&& std::fabs(NormalizeAxis(Yaw   - Other.Yaw))   <= Tolerance
			&& std::fabs(NormalizeAxis(Roll   - Other.Roll))  <= Tolerance;
	}

	/** 허용 오차 내에서 항등 회전에 가까운지 반환합니다. */
	bool IsNearlyZero(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::fabs(NormalizeAxis(Pitch)) <= Tolerance
			&& std::fabs(NormalizeAxis(Yaw))   <= Tolerance
			&& std::fabs(NormalizeAxis(Roll))  <= Tolerance;
	}

	/** 다른 로테이터와의 축별 각도 차이 절댓값 합을 반환합니다. */
	float GetManhattanDistance(const FRotator& Other) const noexcept
	{
		return std::fabs(NormalizeAxis(Pitch - Other.Pitch))
			+  std::fabs(NormalizeAxis(Yaw   - Other.Yaw))
			+  std::fabs(NormalizeAxis(Roll   - Other.Roll));
	}

	/** 전달된 로테이터를 현재 로테이터에 가장 가까운 각도 표현으로 맞춥니다. */
	void SetClosestToMe(FRotator& MakeClosest) const noexcept
	{
		MakeClosest.Pitch = Pitch + NormalizeAxis(MakeClosest.Pitch - Pitch);
		MakeClosest.Yaw   = Yaw   + NormalizeAxis(MakeClosest.Yaw   - Yaw);
		MakeClosest.Roll  = Roll  + NormalizeAxis(MakeClosest.Roll  - Roll);
	}

	/** 벡터를 이 회전만큼 회전시킵니다. (Quat.h에 구현됨) */
	FVector   RotateVector(const FVector& InVector) const noexcept;

	/** 벡터를 이 회전의 반대만큼 회전시킵니다. (Quat.h에 구현됨) */
	FVector   UnrotateVector(const FVector& InVector) const noexcept;

	/** 역회전을 반환합니다. (Quat.h에 구현됨) */
	FRotator  GetInverse() const noexcept;

	/** 쿼터니언으로 변환합니다. (Quat.h에 구현됨) */
	FQuat     Quaternion() const noexcept;
};

/** 제로 로테이터 상수 */
inline constexpr FRotator FRotator::ZeroRotator { 0.0f, 0.0f, 0.0f };

/** 좌측 스칼라 곱셈 연산자 */
inline FRotator operator*(float Scale, const FRotator& Rotator) noexcept
{
	return Rotator * Scale;
}
