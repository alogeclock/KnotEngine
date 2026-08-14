#include "Core/Geometry/Transform.h"

#include "Core/Math/Matrix.h"

#include <cmath>

namespace
{
    FVector ComponentDivideSafe(const FVector& Numerator, const FVector& Denominator,
                                float Tolerance = KMath::Epsilon) noexcept
    {
        return FVector(
            std::fabs(Denominator.X) > Tolerance ? Numerator.X / Denominator.X : 0.0f,
            std::fabs(Denominator.Y) > Tolerance ? Numerator.Y / Denominator.Y : 0.0f,
            std::fabs(Denominator.Z) > Tolerance ? Numerator.Z / Denominator.Z : 0.0f);
    }
} // namespace

const FTransform FTransform::Identity;

FTransform::FTransform(const FQuat& InRotation, const FVector& InTranslation,
                       const FVector& InScale3D) noexcept
    : Rotation(InRotation.GetNormalized()), Translation(InTranslation), Scale3D(InScale3D)
{
}

FTransform FTransform::operator*(const FTransform& Other) const noexcept
{
    return FTransform(
        Rotation * Other.Rotation,
        Other.TransformPosition(Translation),
        Scale3D * Other.Scale3D);
}

FTransform& FTransform::operator*=(const FTransform& Other) noexcept
{
    *this = *this * Other;
    return *this;
}

FVector FTransform::TransformPosition(const FVector& Position) const noexcept
{
    return Rotation.RotateVector(Position * Scale3D) + Translation;
}

FVector FTransform::TransformVector(const FVector& Vector) const noexcept
{
    return Rotation.RotateVector(Vector * Scale3D);
}

FVector FTransform::InverseTransformPosition(const FVector& Position) const noexcept
{
    return ComponentDivideSafe(Rotation.UnrotateVector(Position - Translation), Scale3D);
}

FVector FTransform::InverseTransformVector(const FVector& Vector) const noexcept
{
    return ComponentDivideSafe(Rotation.UnrotateVector(Vector), Scale3D);
}

FMatrix FTransform::ToMatrix() const noexcept
{
    return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), Scale3D);
}

FTransform FTransform::Inverse() const noexcept
{
    const FVector InverseScale3D = ComponentDivideSafe(FVector::OneVector, Scale3D);
    const FQuat InverseRotation = Rotation.Inverse();
    const FVector InverseTranslation = InverseRotation.RotateVector(-Translation * InverseScale3D);
    return FTransform(InverseRotation, InverseTranslation, InverseScale3D);
}
