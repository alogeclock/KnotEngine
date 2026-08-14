#pragma once

#include "Core/Math/Row.h"

enum class EAxis : uint8
{
    X,
    Y,
    Z
};

struct FMatrix
{
public:
    alignas(16) float M[4][4];
    static const FMatrix Identity;

public:
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

    FRow operator[](int32 Row) noexcept;
    FConstRow operator[](int32 Row) const noexcept;

    bool operator==(const FMatrix& Other) const noexcept;
    bool operator!=(const FMatrix& Other) const noexcept;
    FMatrix operator*(const FMatrix& Other) const noexcept;
    FMatrix& operator*=(const FMatrix& Other) noexcept;

    bool Equals(const FMatrix& Other, float Tolerance = KMath::Epsilon) const noexcept;
    FVector TransformVector(const FVector& Vector) const noexcept;
    FVector TransformPosition(const FVector& Position) const noexcept;
    FVector GetScaledAxis(EAxis Axis) const noexcept;
    FMatrix GetInverse(float Tolerance = KMath::Epsilon) const noexcept;
    bool Decompose(FVector& OutTranslation, FMatrix& OutRotation, FVector& OutScale, float Tolerance = KMath::Epsilon) const noexcept;

    static FMatrix MakeTranslation(const FVector& Translation) noexcept;
    static FMatrix MakeScale(const FVector& Scale) noexcept;
    static FMatrix MakeRotationX(float AngleRad) noexcept;
    static FMatrix MakeRotationY(float AngleRad) noexcept;
    static FMatrix MakeRotationZ(float AngleRad) noexcept;
    static FMatrix MakePerspectiveFovLH(float FovYRad, float AspectRatio, float NearZ, float FarZ) noexcept;
    static FMatrix MakeOrthographicLH(float ViewWidth, float ViewHeight, float NearZ, float FarZ) noexcept;
    static FMatrix MakeViewLookAtLH(const FVector& Eye, const FVector& Target, const FVector& Up = FVector::UpVector) noexcept;
    static FMatrix MakeWorld(const FVector& Translation, const FMatrix& Rotation, const FVector& Scale) noexcept;

private:
    static bool TryInverse(const FMatrix& Matrix, FMatrix& OutInverse, float Tolerance) noexcept;
};
