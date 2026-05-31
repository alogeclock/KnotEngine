#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector2.h"

struct FVector
{
public:
	union
	{
		struct
		{
			float X;
			float Y;
			float Z;
		};

		float Data[3];
	};

	static const FVector ZeroVector;
	static const FVector OneVector;
	static const FVector UpVector;
	static const FVector DownVector;
	static const FVector ForwardVector;
	static const FVector BackwardVector;
	static const FVector RightVector;
	static const FVector LeftVector;
	static const FVector XAxisVector;
	static const FVector YAxisVector;
	static const FVector ZAxisVector;

	static constexpr FVector Zero() { return ZeroVector; }
	static constexpr FVector One() { return OneVector; }
	static constexpr FVector UnitX() { return XAxisVector; }
	static constexpr FVector UnitY() { return YAxisVector; }
	static constexpr FVector UnitZ() { return ZAxisVector; }

	// ──────────── Constructor ────────────
public:
	constexpr FVector() noexcept
		: X(0.f), Y(0.f), Z(0.f)
	{
	}

	constexpr FVector(const float InX, const float InY, const float InZ) noexcept
		: X(InX), Y(InY), Z(InZ)
	{
	}

	explicit FVector(const Float3& InFloat3) noexcept
		: X(InFloat3.x), Y(InFloat3.y), Z(InFloat3.z)
	{
	}

	explicit FVector(DirectX::FXMVECTOR InVector) noexcept
		: X(0.f), Y(0.f), Z(0.f)
	{
		DirectX::XMFLOAT3 Temp;
		DirectX::XMStoreFloat3(&Temp, InVector);
		X = Temp.x;
		Y = Temp.y;
		Z = Temp.z;
	}

	FVector(const FVector&) noexcept = default;
	FVector(FVector&&) noexcept = default;

	// ──────────── Operators ────────────
public:
	FVector& operator=(const FVector&) noexcept = default;
	FVector& operator=(FVector&&) noexcept = default;

	float& operator[](int32_t Index) noexcept
	{
		assert(Index >= 0 && Index < 3);
		return Data[Index];
	}

	const float& operator[](int32_t Index) const noexcept
	{
		assert(Index >= 0 && Index < 3);
		return Data[Index];
	}

	constexpr bool operator==(const FVector& Other) const noexcept
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	constexpr bool operator!=(const FVector& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FVector operator-() const noexcept
	{
		return { -X, -Y, -Z };
	}

	constexpr FVector operator+(const FVector& Other) const noexcept
	{
		return { X + Other.X, Y + Other.Y, Z + Other.Z };
	}

	constexpr FVector operator-(const FVector& Other) const noexcept
	{
		return { X - Other.X, Y - Other.Y, Z - Other.Z };
	}

	constexpr FVector operator*(float Scalar) const noexcept
	{
		return { X * Scalar, Y * Scalar, Z * Scalar };
	}

	constexpr FVector operator*(const FVector& Other) const noexcept
	{
		return { X * Other.X, Y * Other.Y, Z * Other.Z };
	}

	constexpr FVector operator/(float Scalar) const noexcept
	{
		assert(Scalar != 0.f);
		return { X / Scalar, Y / Scalar, Z / Scalar };
	}

	FVector& operator+=(const FVector& Other) noexcept
	{
		X += Other.X;
		Y += Other.Y;
		Z += Other.Z;
		return *this;
	}

	FVector& operator-=(const FVector& Other) noexcept
	{
		X -= Other.X;
		Y -= Other.Y;
		Z -= Other.Z;
		return *this;
	}

	FVector& operator*=(float Scalar) noexcept
	{
		X *= Scalar;
		Y *= Scalar;
		Z *= Scalar;
		return *this;
	}

	FVector& operator/=(float Scalar) noexcept
	{
		assert(Scalar != 0.f);
		X /= Scalar;
		Y /= Scalar;
		Z /= Scalar;
		return *this;
	}

	// ──────────── Methods ────────────
public:
	// 현재 벡터를 DirectX::XMFLOAT3 형식으로 변환
	Float3 ToXMFLOAT3() const noexcept
	{
		return DirectX::XMFLOAT3(X, Y, Z);
	}

	// 현재 벡터를 DirectX::XMVECTOR 형식으로 변환
	XMVector ToXMVector(float W = 0.f) const noexcept
	{
		return DirectX::XMVectorSet(X, Y, Z, W);
	}

	// 허용 오차(Tolerance) 범위 내에서 두 벡터가 같은지 비교함
	bool Equals(const FVector& V, float Tolerance = 1.e-6f) const noexcept
	{
		return DirectX::XMVector3NearEqual(ToXMVector(), V.ToXMVector(), DirectX::XMVectorReplicate(Tolerance));
	}

	// 모든 성분이 정확히 0인지 확인
	bool IsZero() const noexcept
	{
		return X == 0.f && Y == 0.f && Z == 0.f;
	}

	// 모든 성분이 허용 오차(Tolerance) 이하인지 확인
	bool IsNearlyZero(float Tolerance = 1.e-6f) const noexcept
	{
		return DirectX::XMVector3NearEqual(
			ToXMVector(),
			DirectX::XMVectorZero(),
			DirectX::XMVectorReplicate(Tolerance));
	}

	// 벡터 길이의 제곱 값을 구함, 제곱근 연산이 없으므로 Size()보다 빠름
	float SizeSquared() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ToXMVector()));
	}

	// 벡터의 길이(크기)를 구함, 제곱근 연산이 포함되어 느림
	float Size() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(ToXMVector()));
	}

	// XY 평면에서의 벡터 길이 제곱 값을 구함
	float SizeSquared2D() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(ToXMVector()));
	}

	// XY 평면에서의 벡터 길이(크기)를 구함
	float Size2D() const noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector2Length(ToXMVector()));
	}

	// 현재 벡터를 정규화함, 길이가 너무 작으면 영벡터로 변환한 뒤 false 반환
	bool Normalize(float Tolerance = 1.e-8f) noexcept
	{
		const XMVector Vector = ToXMVector();
		const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector));
		if (SquareSum > Tolerance)
		{
			*this = FVector(DirectX::XMVector3Normalize(Vector));
			return true;
		}

		X = 0.f;
		Y = 0.f;
		Z = 0.f;
		return false;
	}

	// 정규화된 벡터를 반환함, 기이가 너무 작으면 ZeroVector를 반환함
	FVector GetSafeNormal(float Tolerance = 1.e-8f) const noexcept
	{
		const XMVector Vector = ToXMVector();
		const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Vector));
		if (SquareSum > Tolerance)
		{
			return FVector(DirectX::XMVector3Normalize(Vector));
		}

		return ZeroVector;
	}

	// Backward compatibility aliases
	FVector Normalized(float Tolerance = 1.e-8f) const noexcept { return GetSafeNormal(Tolerance); }
	bool NormalizeSafe(float Tolerance = 1.e-8f) noexcept { return Normalize(Tolerance); }

	float DotProduct(const FVector& Other) const noexcept { return FVector::DotProduct(*this, Other); }
	FVector CrossProduct(const FVector& Other) const noexcept { return FVector::CrossProduct(*this, Other); }

	// XY 평면 기준으로 정규화된 벡터를 반환함, Z는 0으로 설정되며 길이가 너무 작을 경우 ZeroVector 반환
	FVector GetSafeNormal2D(float Tolerance = 1.e-8f) const noexcept
	{
		const XMVector Vector = ToXMVector();
		const float SquareSum = DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(Vector));
		if (SquareSum > Tolerance)
		{
			const XMVector Normalized = DirectX::XMVector2Normalize(Vector);
			return FVector(
				DirectX::XMVectorGetX(Normalized),
				DirectX::XMVectorGetY(Normalized),
				0.0f);
		}

		return ZeroVector;
	}

public:
	// 두 벡터의 내적(Dot Product)을 구함
	static float DotProduct(const FVector& A, const FVector& B) noexcept
	{
		return DirectX::XMVectorGetX(DirectX::XMVector3Dot(A.ToXMVector(), B.ToXMVector()));
	}

	// 두 벡터의 외적(Cross Product)을 구함
	static FVector CrossProduct(const FVector& A, const FVector& B) noexcept
	{
		return FVector(DirectX::XMVector3Cross(A.ToXMVector(), B.ToXMVector()));
	}

	// 두 벡터 사이 거리의 제곱 값을 구함, 거리 비교만 필요할 때 Dist()보다 효율적임
	static float DistSquared(const FVector& A, const FVector& B) noexcept
	{
		const XMVector Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
		return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Delta));
	}

	// 두 벡터의 거리를 구함
	static float Dist(const FVector& A, const FVector& B) noexcept
	{
		const XMVector Delta = DirectX::XMVectorSubtract(A.ToXMVector(), B.ToXMVector());
		return DirectX::XMVectorGetX(DirectX::XMVector3Length(Delta));
	}

	// Backward compatibility alias
	static float Distance(const FVector& A, const FVector& B) noexcept { return Dist(A, B); }
};

// --- Definitions of static members ---
inline const FVector FVector::ZeroVector{ 0.f, 0.f, 0.f };
inline const FVector FVector::OneVector{ 1.f, 1.f, 1.f };
inline const FVector FVector::UpVector{ 0.f, 1.f, 0.f };
inline const FVector FVector::DownVector{ 0.f, -1.f, 0.f };
inline const FVector FVector::ForwardVector{ 1.f, 0.f, 0.f };
inline const FVector FVector::BackwardVector{ -1.f, 0.f, 0.f };
inline const FVector FVector::RightVector{ 1.f, 0.f, 0.f };
inline const FVector FVector::LeftVector{ -1.f, 0.f, 0.f };
inline const FVector FVector::XAxisVector{ 1.f, 0.f, 0.f };
inline const FVector FVector::YAxisVector{ 0.f, 1.f, 0.f };
inline const FVector FVector::ZAxisVector{ 0.f, 0.f, 1.f };

// Vector2 CrossProduct Implementation
inline FVector FVector2::CrossProduct(const FVector2& A, const FVector2& B) noexcept
{
	return FVector(DirectX::XMVector2Cross(A.ToXMVector(), B.ToXMVector()));
}

namespace std
{
template <>
struct hash<FVector>
{
	size_t operator()(const FVector& V) const noexcept
	{
		size_t H = std::hash<float>{}(V.X);
		H ^= std::hash<float>{}(V.Y) * 2654435761u + 0x9e3779b9u + (H << 6) + (H >> 2);
		H ^= std::hash<float>{}(V.Z) * 2654435761u + 0x9e3779b9u + (H << 6) + (H >> 2);
		return H;
	}
};
}
