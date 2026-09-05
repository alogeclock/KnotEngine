#pragma once

#include "EngineAPI.h"

#include <cmath>

#include "Core/Math/Vector4.h"

enum class EAxis : uint8
{
	X,
	Y,
	Z
};

struct ENGINE_API FMatrix
{
	// 멤버 변수 (Member Variables)
	alignas(16) float M[4][4];
	static const FMatrix Identity;

	// 생성자 (Constructors)
	constexpr FMatrix() noexcept
	    : M{ { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
	{
	}

	constexpr FMatrix(
	    float M00, float M01, float M02, float M03,
	    float M10, float M11, float M12, float M13,
	    float M20, float M21, float M22, float M23,
	    float M30, float M31, float M32, float M33) noexcept
	    : M{ { M00, M01, M02, M03 },
		     { M10, M11, M12, M13 },
		     { M20, M21, M22, M23 },
		     { M30, M31, M32, M33 } }
	{
	}

	constexpr FMatrix(const FVector4& Row0, const FVector4& Row1, const FVector4& Row2, const FVector4& Row3) noexcept
	    : M{ { Row0.X, Row0.Y, Row0.Z, Row0.W },
		     { Row1.X, Row1.Y, Row1.Z, Row1.W },
		     { Row2.X, Row2.Y, Row2.Z, Row2.W },
		     { Row3.X, Row3.Y, Row3.Z, Row3.W } }
	{
	}

	// 요소 접근 연산자 (Element Access Operators)
	constexpr auto operator[](int32 Row) & noexcept -> float (&)[4]
	{
		check(Row >= 0 && Row < 4);
		return M[Row];
	}

	constexpr auto operator[](int32 Row) const & noexcept -> const float (&)[4]
	{
		check(Row >= 0 && Row < 4);
		return M[Row];
	}

	void operator[](int32 Row) && = delete;
	void operator[](int32 Row) const && = delete;

	// 비교 및 행렬 연산자 (Comparison and Matrix Operators)
	constexpr bool operator==(const FMatrix& Other) const noexcept
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (M[Row][Column] != Other.M[Row][Column])
				{
					return false;
				}
			}
		}
		return true;
	}

	constexpr bool operator!=(const FMatrix& Other) const noexcept
	{
		return !(*this == Other);
	}

	constexpr FMatrix operator*(const FMatrix& Other) const noexcept
	{
		FMatrix Result;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				Result.M[Row][Column] =
					M[Row][0] * Other.M[0][Column] +
					M[Row][1] * Other.M[1][Column] +
					M[Row][2] * Other.M[2][Column] +
					M[Row][3] * Other.M[3][Column];
			}
		}
		return Result;
	}

	constexpr FMatrix& operator*=(const FMatrix& Other) noexcept
	{
		*this = *this * Other;
		return *this;
	}

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	bool Equals(const FMatrix& Other, float Tolerance = KMath::Epsilon) const noexcept
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Column = 0; Column < 4; ++Column)
			{
				if (std::fabs(M[Row][Column] - Other.M[Row][Column]) > Tolerance)
				{
					return false;
				}
			}
		}
		return true;
	}

	constexpr FVector TransformVector(const FVector& Vector) const noexcept
	{
		return {
			Vector.X * M[0][0] + Vector.Y * M[1][0] + Vector.Z * M[2][0],
			Vector.X * M[0][1] + Vector.Y * M[1][1] + Vector.Z * M[2][1],
			Vector.X * M[0][2] + Vector.Y * M[1][2] + Vector.Z * M[2][2],
		};
	}

	FVector TransformPosition(const FVector& Position) const noexcept
	{
		const FVector4 Result = FVector4(Position, 1.0f) * *this;
		return Result.ToVector3();
	}

	constexpr FVector GetScaledAxis(EAxis Axis) const noexcept
	{
		switch (Axis)
		{
		case EAxis::X: return { M[0][0], M[0][1], M[0][2] };
		case EAxis::Y: return { M[1][0], M[1][1], M[1][2] };
		case EAxis::Z: return { M[2][0], M[2][1], M[2][2] };
		default: return FVector::ZeroVector;
		}
	}

	FMatrix GetInverse(float Tolerance = KMath::Epsilon) const noexcept;
	bool Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance = KMath::Epsilon) const noexcept;

	// 공용 행렬 생성 함수 (Static Matrix Creation Functions)
	static FMatrix MakeTranslation(const FVector& Translation) noexcept;
	static FMatrix MakeScale(const FVector& Scale) noexcept;
	static FMatrix MakeRotationX(float AngleRad) noexcept;
	static FMatrix MakeRotationY(float AngleRad) noexcept;
	static FMatrix MakeRotationZ(float AngleRad) noexcept;
	static FMatrix MakePerspectiveFov(float FovYRad, float AspectRatio, float NearZ, float FarZ) noexcept;
	static FMatrix MakeOrthographic(float ViewWidth, float ViewHeight, float NearZ, float FarZ) noexcept;
	static FMatrix MakeLookAt(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept;
	static FMatrix MakeWorld(const FVector& Translation, const FMatrix& Rotation, const FVector& Scale) noexcept;

private:
	// 내부 헬퍼 함수 (Private Helper Functions)
	static bool TryInverse(const FMatrix& Matrix, FMatrix& OutInverse, float Tolerance) noexcept;
};

constexpr FVector4 FVector4::operator*(const FMatrix& Matrix) const noexcept
{
	return {
		X * Matrix.M[0][0] + Y * Matrix.M[1][0] + Z * Matrix.M[2][0] + W * Matrix.M[3][0],
		X * Matrix.M[0][1] + Y * Matrix.M[1][1] + Z * Matrix.M[2][1] + W * Matrix.M[3][1],
		X * Matrix.M[0][2] + Y * Matrix.M[1][2] + Z * Matrix.M[2][2] + W * Matrix.M[3][2],
		X * Matrix.M[0][3] + Y * Matrix.M[1][3] + Z * Matrix.M[2][3] + W * Matrix.M[3][3],
	};
}
