#pragma once

#include <initializer_list>

#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

struct Vector4;

enum class EAxis : uint8_t
{
	X,
	Y,
	Z,
	Center
};

struct FMatrix
{
public:
	struct FConstRow;

	struct FRow
	{
		float* Values;

		float& operator[](int32 Column) noexcept
		{
			check(Column >= 0 && Column < 4);
			return Values[Column];
		}

		const float& operator[](int32 Column) const noexcept
		{
			check(Column >= 0 && Column < 4);
			return Values[Column];
		}

		FRow& operator=(const FRow& Row) noexcept
		{
			Values[0] = Row.Values[0];
			Values[1] = Row.Values[1];
			Values[2] = Row.Values[2];
			Values[3] = Row.Values[3];
			return *this;
		}

		FRow& operator=(const FConstRow& Row) noexcept;

		FRow& operator=(const FVector4& Row) noexcept
		{
			Values[0] = Row.X;
			Values[1] = Row.Y;
			Values[2] = Row.Z;
			Values[3] = Row.W;
			return *this;
		}

		FRow& operator=(const Float4& Row) noexcept
		{
			Values[0] = Row.x;
			Values[1] = Row.y;
			Values[2] = Row.z;
			Values[3] = Row.w;
			return *this;
		}

		FRow& operator=(std::initializer_list<float> Row) noexcept
		{
			check(Row.size() == 4);
			int32 Column = 0;
			for (const float Value : Row)
			{
				if (Column >= 4)
				{
					break;
				}
				Values[Column++] = Value;
			}
			return *this;
		}

		operator FVector4() const noexcept
		{
			return FVector4(Values[0], Values[1], Values[2], Values[3]);
		}

		operator float*() noexcept { return Values; }
		operator const float*() const noexcept { return Values; }

		FRow& operator+=(const FVector4& Row) noexcept
		{
			Values[0] += Row.X;
			Values[1] += Row.Y;
			Values[2] += Row.Z;
			Values[3] += Row.W;
			return *this;
		}

		FRow& operator-=(const FVector4& Row) noexcept
		{
			Values[0] -= Row.X;
			Values[1] -= Row.Y;
			Values[2] -= Row.Z;
			Values[3] -= Row.W;
			return *this;
		}

		FRow& operator*=(float Scalar) noexcept
		{
			Values[0] *= Scalar;
			Values[1] *= Scalar;
			Values[2] *= Scalar;
			Values[3] *= Scalar;
			return *this;
		}

		FRow& operator/=(float Scalar) noexcept
		{
			check(Scalar != 0.f);
			const float InvScalar = 1.0f / Scalar;
			return *this *= InvScalar;
		}

		FVector4 operator+(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] + Row.X, Values[1] + Row.Y, Values[2] + Row.Z, Values[3] + Row.W);
		}

		FVector4 operator-(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] - Row.X, Values[1] - Row.Y, Values[2] - Row.Z, Values[3] - Row.W);
		}

		FVector4 operator*(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] * Row.X, Values[1] * Row.Y, Values[2] * Row.Z, Values[3] * Row.W);
		}

		FVector4 operator/(const FVector4& Row) const noexcept
		{
			check(Row.X != 0.f && Row.Y != 0.f && Row.Z != 0.f && Row.W != 0.f);
			return FVector4(Values[0] / Row.X, Values[1] / Row.Y, Values[2] / Row.Z, Values[3] / Row.W);
		}

		FVector4 operator*(float Scalar) const noexcept
		{
			return FVector4(Values[0] * Scalar, Values[1] * Scalar, Values[2] * Scalar, Values[3] * Scalar);
		}

		FVector4 operator/(float Scalar) const noexcept
		{
			check(Scalar != 0.f);
			const float InvScalar = 1.0f / Scalar;
			return *this * InvScalar;
		}
	};

	struct FConstRow
	{
		const float* Values;

		const float& operator[](int32 Column) const noexcept
		{
			check(Column >= 0 && Column < 4);
			return Values[Column];
		}

		operator FVector4() const noexcept
		{
			return FVector4(Values[0], Values[1], Values[2], Values[3]);
		}

		operator const float*() const noexcept { return Values; }

		FVector4 operator+(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] + Row.X, Values[1] + Row.Y, Values[2] + Row.Z, Values[3] + Row.W);
		}

		FVector4 operator-(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] - Row.X, Values[1] - Row.Y, Values[2] - Row.Z, Values[3] - Row.W);
		}

		FVector4 operator*(const FVector4& Row) const noexcept
		{
			return FVector4(Values[0] * Row.X, Values[1] * Row.Y, Values[2] * Row.Z, Values[3] * Row.W);
		}

		FVector4 operator/(const FVector4& Row) const noexcept
		{
			check(Row.X != 0.f && Row.Y != 0.f && Row.Z != 0.f && Row.W != 0.f);
			return FVector4(Values[0] / Row.X, Values[1] / Row.Y, Values[2] / Row.Z, Values[3] / Row.W);
		}

		FVector4 operator*(float Scalar) const noexcept
		{
			return FVector4(Values[0] * Scalar, Values[1] * Scalar, Values[2] * Scalar, Values[3] * Scalar);
		}

		FVector4 operator/(float Scalar) const noexcept
		{
			check(Scalar != 0.f);
			const float InvScalar = 1.0f / Scalar;
			return *this * InvScalar;
		}
	};

	alignas(16) float M[4][4];

	static const FMatrix Identity;

	// ──────────── Constructor ────────────
public:
	constexpr FMatrix() noexcept
		: M{ { 1.f, 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f, 0.f }, { 0.f, 0.f, 0.f, 1.f } }
	{
	}

	constexpr FMatrix(float M00, float M01, float M02, float M03, float M10, float M11, float M12,
					  float M13, float M20, float M21, float M22, float M23, float M30, float M31,
					  float M32, float M33) noexcept
		: M{ { M00, M01, M02, M03 }, { M10, M11, M12, M13 }, { M20, M21, M22, M23 }, { M30, M31, M32, M33 } }
	{
	}

	constexpr FMatrix(const FVector4& Row0, const FVector4& Row1, const FVector4& Row2,
					  const FVector4& Row3) noexcept
		: M{ { Row0.X, Row0.Y, Row0.Z, Row0.W },
			 { Row1.X, Row1.Y, Row1.Z, Row1.W },
			 { Row2.X, Row2.Y, Row2.Z, Row2.W },
			 { Row3.X, Row3.Y, Row3.Z, Row3.W } }
	{
	}

	explicit FMatrix(CXMMatrix InMatrix) noexcept
	{
		Float4X4 Temp;
		DirectX::XMStoreFloat4x4(&Temp, InMatrix);

		M[0][0] = Temp._11;
		M[0][1] = Temp._12;
		M[0][2] = Temp._13;
		M[0][3] = Temp._14;
		M[1][0] = Temp._21;
		M[1][1] = Temp._22;
		M[1][2] = Temp._23;
		M[1][3] = Temp._24;
		M[2][0] = Temp._31;
		M[2][1] = Temp._32;
		M[2][2] = Temp._33;
		M[2][3] = Temp._34;
		M[3][0] = Temp._41;
		M[3][1] = Temp._42;
		M[3][2] = Temp._43;
		M[3][3] = Temp._44;
	}

	FMatrix(const FMatrix&) noexcept = default;
	FMatrix(FMatrix&&) noexcept = default;
	FMatrix& operator=(const FMatrix&) noexcept = default;
	FMatrix& operator=(FMatrix&&) noexcept = default;

public:
	FRow operator[](int32 Row) noexcept
	{
		check(Row >= 0 && Row < 4);
		return FRow{ M[Row] };
	}
	FConstRow operator[](int32 Row) const noexcept
	{
		check(Row >= 0 && Row < 4);
		return FConstRow{ M[Row] };
	}

	// operator==는 부동소수점 정확 비교입니다.
	// 계산 결과 비교에는 Equals(Tolerance)를 사용하는 것을 권장합니다.
	bool operator==(const FMatrix& Other) const noexcept
	{
		for (int32 Row = 0; Row < 4; ++Row)
		{
			const XMVector ThisRow =
				DirectX::XMVectorSet(M[Row][0], M[Row][1], M[Row][2], M[Row][3]);
			const XMVector OtherRow = DirectX::XMVectorSet(Other.M[Row][0], Other.M[Row][1],
														   Other.M[Row][2], Other.M[Row][3]);
			if (!DirectX::XMVector4Equal(ThisRow, OtherRow))
			{
				return false;
			}
		}
		return true;
	}

	bool operator!=(const FMatrix& Other) const noexcept { return !(*this == Other); }

	FMatrix operator-() const noexcept
	{
		return FMatrix(-M[0][0], -M[0][1], -M[0][2], -M[0][3], -M[1][0], -M[1][1], -M[1][2],
					   -M[1][3], -M[2][0], -M[2][1], -M[2][2], -M[2][3], -M[3][0], -M[3][1],
					   -M[3][2], -M[3][3]);
	}

	FMatrix operator+(const FMatrix& Other) const noexcept;
	FMatrix operator-(const FMatrix& Other) const noexcept;
	FMatrix operator*(float Scalar) const noexcept;
	FMatrix operator/(float Scalar) const noexcept;
	FMatrix& operator+=(const FMatrix& Other) noexcept;
	FMatrix& operator-=(const FMatrix& Other) noexcept;
	FMatrix& operator*=(float Scalar) noexcept;
	FMatrix& operator/=(float Scalar) noexcept;
	FMatrix operator*(const FMatrix& Other) const noexcept;
	FMatrix& operator*=(const FMatrix& Other) noexcept;

	// ──────────── method ────────────
public:
	XMMatrix ToXMMatrix() const noexcept
	{
		return DirectX::XMMATRIX(M[0][0], M[0][1], M[0][2], M[0][3],
								 M[1][0], M[1][1], M[1][2], M[1][3],
								 M[2][0], M[2][1], M[2][2], M[2][3],
								 M[3][0], M[3][1], M[3][2], M[3][3]);
	}

	// 두 행렬이 허용 오차(Tolerance) 범위 내에서 같은지 비교함
	bool Equals(const FMatrix& Other, float Tolerance = 1.e-6f) const noexcept;

	// 전치 행렬(Transpose Matrix)을 반환함
	FMatrix GetTransposed() const noexcept;

	// 방향 벡터를 현재 행렬로 변환함
	// 이동(Translation)은 적용하지 않음
	FVector TransformVector(const FVector& V) const noexcept;

	// 위치 벡터를 현재 행렬로 변환, 이동(Translation)을 포함하여 적용함
	FVector TransformPosition(const FVector& V) const noexcept;

	FVector TransformPositionWithW(const FVector& V) const noexcept;

	FVector4 TransformVector4(const FVector4& V, const FMatrix& M) const noexcept;

	// 현재 행렬의 이동(Translation) 성분을 반환함
	FVector GetOrigin() const noexcept { return FVector(M[3][0], M[3][1], M[3][2]); }

	// 현재 행렬의 이동(Translation) 성분을 설정함
	void SetOrigin(const FVector& Origin) noexcept
	{
		(*this)[3] = FVector4(Origin, M[3][3]);
	}

	// 현재 행렬에서 스케일이 포함된 축 벡터를 반환함
	FVector GetScaledAxis(EAxis Axis) const noexcept
	{
		switch (Axis)
		{
		case EAxis::X:
			return FVector(M[0][0], M[0][1], M[0][2]);
		case EAxis::Y:
			return FVector(M[1][0], M[1][1], M[1][2]);
		case EAxis::Z:
			return FVector(M[2][0], M[2][1], M[2][2]);
		default:
			return FVector::ZeroVector;
		}
	}

	// 현재 행렬에서 정규화된 축 벡터를 반환함
	FVector GetUnitAxis(EAxis Axis) const noexcept { return GetScaledAxis(Axis).GetSafeNormal(); }

	// 현재 행렬에서 이동(Translation) 성분을 제거함
	void RemoveTranslation() noexcept
	{
		(*this)[3] = FVector4(FVector::Zero(), M[3][3]);
	}

	// 이동(Translation) 성분이 제거된 새 행렬을 반환함
	FMatrix GetMatrixWithoutTranslation() const noexcept
	{
		FMatrix Result = *this;
		Result.RemoveTranslation();
		return Result;
	}

	// 현재 행렬에서 스케일을 제거한 새 행렬을 반환함
	FMatrix GetMatrixWithoutScale(float Tolerance = 1.e-8f) const noexcept
	{
		const FVector XAxis = GetScaledAxis(EAxis::X).GetSafeNormal(Tolerance);
		const FVector YAxis = GetScaledAxis(EAxis::Y).GetSafeNormal(Tolerance);
		const FVector ZAxis = GetScaledAxis(EAxis::Z).GetSafeNormal(Tolerance);

		FMatrix Result = *this;
		Result[0] = FVector4(XAxis, M[0][3]);
		Result[1] = FVector4(YAxis, M[1][3]);
		Result[2] = FVector4(ZAxis, M[2][3]);

		return Result;
	}

	// 현재 행렬에 포함된 스케일 값을 반환함
	FVector GetScaleVector() const noexcept;

	// 현재 행렬이 Identity Matrix와 같은지 확인함
	bool IsIdentity(float Tolerance = 1.e-6f) const noexcept { return Equals(Identity, Tolerance); }

	// 현재 행렬의 행렬식(Determinant)을 구함
	// Determinant가 0이면 역행렬  없음
	// 0에 매우 가까우면 수치적으로 불안정
	// Inverse 전에 판단할 때 중요한 함수
	float Determinant() const noexcept;

	// 현재 행렬의 역행렬(Inverse Matrix)을 반환함
	// 역행렬이 존재하지 않으면 Identity를 반환함
	// 현재는 역행렬이 없을 때 Identity를 반환/대입하는 정책입니다.
	// 디버깅 투명성을 높이려면 원본 유지 + false 반환 정책도 고려할 수 있습니다.
	FMatrix GetInverse(float Tolerance = 1.e-8f) const noexcept;

	// 현재 행렬을 역행렬로 변환함
	// 역행렬이 존재하지 않으면 Identity로 설정함
	// 현재는 역행렬이 없을 때 Identity를 반환/대입하는 정책입니다.
	// 디버깅 투명성을 높이려면 원본 유지 + false 반환 정책도 고려할 수 있습니다.
	[[nodiscard]] bool Inverse(float Tolerance = 1.e-8f) noexcept;

	// 현재 행렬이 역행렬을 가질 수 있는지 확인함
	bool IsInvertible(float Tolerance = 1.e-8f) const noexcept;

	// 현재 행렬에 스케일을 적용한 새 행렬을 반환함
	FMatrix ApplyScale(const FVector& Scale) const noexcept { return MakeScale(Scale) * *this; }

	// 현재 행렬에 균일 스케일을 적용한 새 행렬을 반환함
	FMatrix ApplyScale(float Scale) const noexcept
	{
		return ApplyScale(FVector(Scale, Scale, Scale));
	}

	// 현재 행렬에서 순수 회전 행렬을 반환함
	FMatrix GetRotationMatrix(float Tolerance = 1.e-8f) const noexcept
	{
		return GetMatrixWithoutTranslation().GetMatrixWithoutScale(Tolerance);
	}

	// 현재 행렬의 Forward 방향 벡터를 반환함
	FVector GetForward() const noexcept { return GetUnitAxis(EAxis::X); }

	// 현재 행렬의 Right 방향 벡터를 반환함
	FVector GetRight() const noexcept { return GetUnitAxis(EAxis::Y); }

	// 현재 행렬의 Up 방향 벡터를 반환함
	FVector GetUp() const noexcept { return GetUnitAxis(EAxis::Z); }

	// 축 벡터와 위치를 이용하여 현재 행렬을 설정함
	void SetAxes(const FVector& XAxis, const FVector& YAxis, const FVector& ZAxis, const FVector& Origin = FVector::ZeroVector) noexcept
	{
		(*this)[0] = FVector4(XAxis, 0.0f);
		(*this)[1] = FVector4(YAxis, 0.0f);
		(*this)[2] = FVector4(ZAxis, 0.0f);
		(*this)[3] = FVector4(Origin, 1.0f);
	}

	// 현재 행렬을 위치, 회전 행렬, 스케일로 분해함, 분해에 실패하면 false를 반환함
	bool Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance = 1.e-8f) const noexcept;

	// 현재 행렬의 위치(Translation) 성분을 반환함
	FVector GetTranslation() const noexcept { return GetOrigin(); }

	// 현재 행렬의 위치(Translation) 성분을 설정함
	void SetTranslation(const FVector& Translation) noexcept { SetOrigin(Translation); }

	// 이동(Translation) 행렬을 생성함
	static FMatrix MakeTranslation(const FVector& Translation) noexcept;

	// 스케일(Scale) 행렬을 생성함
	static FMatrix MakeScale(const FVector& Scale) noexcept;

	// X축 기준 회전 행렬을 생성함
	static FMatrix MakeRotationX(float AngleRad) noexcept;

	// Y축 기준 회전 행렬을 생성함
	static FMatrix MakeRotationY(float AngleRad) noexcept;

	// Z축 방향 벡터를 기준으로 직교 기저 행렬을 생성함, 이 방향을 Z축(Up/Normal)으로 사용하는 함수임
	static FMatrix MakeRotationZ(float AngleRad) noexcept;

	// 단일 스칼라 값으로 균일 스케일 행렬을 생성함
	static FMatrix MakeScale(float Scale) noexcept;

	static FMatrix MakeScaleMatrix(float Scale) noexcept;

	static FMatrix MakeRotationAxis(const FVector& Axis, float AngleRad) noexcept;

	static FMatrix MakeRotationEuler(const FVector& EulerDegrees) noexcept;

	FVector GetEuler() const noexcept;

	// X축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	static FMatrix MakeFromX(const FVector& XAxis) noexcept;

	// Y축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	static FMatrix MakeFromY(const FVector& YAxis) noexcept;

	// Z축 방향 벡터를 기준으로 직교 기저 행렬을 생성함
	static FMatrix MakeFromZ(const FVector& ZAxis) noexcept;

	// Eye 위치에서 Target 위치를 바라보는 LookAt 행렬을 생성함
	static FMatrix MakeLookAt(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept;

	// Left-Handed 기준 원근 투영 행렬을 생성함
	static FMatrix MakePerspectiveFovLH(float FovYRad, float AspectRatio, float NearZ,
										float FarZ) noexcept;

	// Left-Handed 기준 직교 투영 행렬을 생성함
	static FMatrix MakeOrthographicLH(float ViewWidth, float ViewHeight, float NearZ,
									  float FarZ) noexcept;

	// Left-Handed 기준 View LookAt 행렬을 생성함
	static FMatrix MakeViewLookAtLH(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept;

	// 지정한 위치에서 카메라를 바라보는 Billboard 행렬을 생성함
	static FMatrix MakeBillboard(const FVector& Position, const FVector& CameraPosition, const FVector& Up = FVector::UpVector) noexcept;

	// 위치, 회전 행렬, 스케일을 이용하여 월드 행렬을 생성
	static FMatrix MakeWorld(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept;

	// 위치(Translation), 회전(Rotation), 스케일(Scale)로 행렬을 생성함
	static FMatrix MakeTRS(const FVector& Translation, const FMatrix& RotationMatrix, const FVector& Scale) noexcept;
};

inline FMatrix operator*(float Scalar, const FMatrix& Matrix) noexcept
{
	return Matrix * Scalar;
}
inline FVector4 operator*(float Scalar, const FMatrix::FRow& Row) noexcept
{
	return Row * Scalar;
}
inline FVector4 operator*(float Scalar, const FMatrix::FConstRow& Row) noexcept
{
	return Row * Scalar;
}

inline FMatrix::FRow& FMatrix::FRow::operator=(const FMatrix::FConstRow& Row) noexcept
{
	Values[0] = Row.Values[0];
	Values[1] = Row.Values[1];
	Values[2] = Row.Values[2];
	Values[3] = Row.Values[3];
	return *this;
}

inline constexpr FMatrix FMatrix::Identity(
	1.f, 0.f, 0.f, 0.f,
	0.f, 1.f, 0.f, 0.f,
	0.f, 0.f, 1.f, 0.f,
	0.f, 0.f, 0.f, 1.f);
