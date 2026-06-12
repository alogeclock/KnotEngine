#pragma once

#include <cassert>
#include <cmath>

#include "Core/CoreTypes.h"
#include "Core/Math/Utils.h"
#include "Core/Math/Vector.h"

struct FMatrix;

/**
 * 4차원 벡터 구조체(X, Y, Z, W).
 * 동차 좌표계(Homogeneous Coordinates)를 지원하며 점(Point, W=1)과 벡터(Vector, W=0)를 구분합니다.
 */
struct FVector4
{
public:
	union
	{
		struct { float X, Y, Z, W; };
		float XYZW[4];
	};

	// ──────────── Constructors ────────────
public:
	/** 기본 생성자 및 성분별 생성자 */
	constexpr FVector4(float InX = 0.0f, float InY = 0.0f, float InZ = 0.0f, float InW = 0.0f) noexcept
		: X(InX), Y(InY), Z(InZ), W(InW)
	{
	}

	/** FVector와 W 성분을 이용한 생성자 */
	constexpr FVector4(const FVector& InVec, float InW = 0.0f) noexcept
		: X(InVec.X), Y(InVec.Y), Z(InVec.Z), W(InW)
	{
	}

	~FVector4() = default;
	
	// ──────────── Static Helpers ────────────
public:
	/** 영벡터 (0,0,0,0) */
	static constexpr FVector4 ZeroVector() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
	/** 영점 (0,0,0,1) */
	static constexpr FVector4 ZeroPoint() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
	/** 상단 벡터 (0,0,1,0) */
	static constexpr FVector4 UpVector() { return { 0.0f, 0.0f, 1.0f, 0.0f }; }
	/** 우측 벡터 (0,1,0,0) */
	static constexpr FVector4 RightVector() { return { 0.0f, 1.0f, 0.0f, 0.0f }; }
	/** 전방 벡터 (1,0,0,0) */
	static constexpr FVector4 ForwardVector() { return { 1.0f, 0.0f, 0.0f, 0.0f }; }

	static constexpr FVector4 Zero()    { return ZeroVector(); }
	static constexpr FVector4 Up()      { return UpVector(); }
	static constexpr FVector4 Right()   { return RightVector(); }
	static constexpr FVector4 Forward() { return ForwardVector(); }
	static constexpr FVector4 Point()   { return ZeroPoint(); }

	/** 특정 좌표의 점(W=1)을 생성합니다. */
	static constexpr FVector4 Point(float InX, float InY, float InZ)
	{
		return { InX, InY, InZ, 1.0f };
	}

	/** 특정 좌표의 벡터(W=0)를 생성합니다. */
	static constexpr FVector4 Vector(float InX, float InY, float InZ)
	{
		return { InX, InY, InZ, 0.0f };
	}

	// ──────────── Operators ────────────
public:
	/** 덧셈 연산자. 점+점은 허용되지 않습니다. */
	FVector4 operator+(const FVector4& Other) const noexcept
	{
		const bool bThisPoint  = IsPoint();
		const bool bOtherPoint = Other.IsPoint();
		assert(!(bThisPoint && bOtherPoint) && "FVector4: Point + Point is invalid.");
		const float ResultW = (bThisPoint || bOtherPoint) ? 1.0f : 0.0f;
		return { X + Other.X, Y + Other.Y, Z + Other.Z, ResultW };
	}

	/** 뺄셈 연산자. 벡터-점은 허용되지 않습니다. */
	FVector4 operator-(const FVector4& Other) const noexcept
	{
		const bool bThisPoint  = IsPoint();
		const bool bOtherPoint = Other.IsPoint();
		assert((bThisPoint || !bOtherPoint) && "FVector4: Vector - Point is invalid.");
		const float ResultW = (bThisPoint && !bOtherPoint) ? 1.0f : 0.0f;
		return { X - Other.X, Y - Other.Y, Z - Other.Z, ResultW };
	}

	/** 스칼라 곱셈 연산자 */
	FVector4 operator*(float S) const noexcept
	{
		return { X * S, Y * S, Z * S, W * S };
	}

	/** 스칼라 나눗셈 연산자 */
	FVector4 operator/(float S) const noexcept
	{
		assert(std::abs(S) >= KMath::Epsilon && "Division by zero in FVector4::operator/");
		const float Inv = 1.0f / S;
		return { X * Inv, Y * Inv, Z * Inv, W * Inv };
	}

	/** 동등 비교 연산자 (허용 오차 사용) */
	bool operator==(const FVector4& Other) const noexcept { return IsNearlyEqual(Other); }

	/** 행렬 곱셈 연산자 */
	FVector4 operator*(const struct FMatrix& Mat) const noexcept;

	// ──────────── Methods ────────────
public:
	/** 3D 벡터 내적 (W 성분 제외) */
	[[nodiscard]] float Dot(const FVector4& Other) const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Dot(ToXMVector(), Other.ToXMVector()));
	}

	/** 3D 벡터 외적 (W 성분은 0으로 설정) */
	[[nodiscard]] FVector4 Cross(const FVector4& Other) const noexcept
	{
		const DirectX::XMVECTOR C = DirectX::XMVector3Cross(ToXMVector(), Other.ToXMVector());
		DirectX::XMFLOAT3 T;
		DirectX::XMStoreFloat3(&T, C);
		return { T.x, T.y, T.z, 0.0f };
	}

	/** 허용 오차 내에서 다른 벡터와 같은지 확인합니다. */
	[[nodiscard]] bool IsNearlyEqual(const FVector4& Other) const noexcept
	{
		return DirectX::XMVector4NearEqual(
			ToXMVector(),
			Other.ToXMVector(),
			DirectX::XMVectorReplicate(KMath::Epsilon));
	}

	/** 3D 방향 벡터 정규화 (W는 0으로 유지) */
	[[nodiscard]] FVector4 Normalize() const noexcept
	{
		const DirectX::XMVECTOR V = DirectX::XMVectorSet(X, Y, Z, 0.0f);
		const float Len = DirectX::XMVectorGetX(DirectX::XMVector3Length(V));
		if (std::abs(Len) < KMath::Epsilon)
		{
			return ZeroVector();
		}
		const DirectX::XMVECTOR N = DirectX::XMVector3Normalize(V);
		DirectX::XMFLOAT3 T;
		DirectX::XMStoreFloat3(&T, N);
		return { T.x, T.y, T.z, 0.0f };
	}

	/** 3D 벡터의 길이 (W 성분 제외) */
	[[nodiscard]] float Length() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSet(X, Y, Z, 0.0f)));
	}

	/** 이 벡터가 점(Point, W=1)인지 확인합니다. */
	[[nodiscard]] bool IsPoint(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::abs(W - 1.0f) <= Tolerance;
	}

	/** 이 벡터가 방향 벡터(Vector, W=0)인지 확인합니다. */
	[[nodiscard]] bool IsVector(float Tolerance = KMath::Epsilon) const noexcept
	{
		return std::abs(W) <= Tolerance;
	}

	/** 동차 좌표계를 3D 벡터로 변환합니다. 점인 경우 Perspective Divide(X/W, Y/W, Z/W)를 수행합니다. */
	[[nodiscard]] FVector ToVector3(float Tolerance = KMath::Epsilon) const noexcept
	{
		if (std::abs(W) <= Tolerance)
		{
			return FVector(X, Y, Z);
		}
		const float InvW = 1.0f / W;
		return FVector(X * InvW, Y * InvW, Z * InvW);
	}

	/** DirectX Math XMVECTOR 형식으로 변환 */
	XMVector ToXMVector() const noexcept { return DirectX::XMVectorSet(X, Y, Z, W); }
};
