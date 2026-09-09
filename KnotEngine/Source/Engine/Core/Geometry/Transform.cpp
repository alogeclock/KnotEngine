#include "Core/Geometry/Transform.h"

#include "Core/Math/Matrix.h"

#include <cmath>

FVector FTransform::ComponentDivideSafe(const FVector& Numerator, const FVector& Denominator, float Tolerance) noexcept
{
	return FVector(
		std::fabs(Denominator.X) > Tolerance ? Numerator.X / Denominator.X : 0.0f,
		std::fabs(Denominator.Y) > Tolerance ? Numerator.Y / Denominator.Y : 0.0f,
		std::fabs(Denominator.Z) > Tolerance ? Numerator.Z / Denominator.Z : 0.0f);
}

const FTransform FTransform::Identity;

FTransform::FTransform(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale) noexcept
	: Rotation(InRotation.GetNormalized()), Translation(InTranslation), Scale(InScale)
{
}

FTransform FTransform::operator*(const FTransform& Other) const noexcept
{
	return FTransform(Other.Rotation * Rotation, Other.TransformPosition(Translation), Scale * Other.Scale);
}

FTransform& FTransform::operator*=(const FTransform& Other) noexcept
{
	*this = *this * Other;
	return *this;
}

FVector FTransform::TransformPosition(const FVector& Position) const noexcept
{
	return Rotation.RotateVector(Position * Scale) + Translation;
}

FVector FTransform::TransformVector(const FVector& Vector) const noexcept
{
	return Rotation.RotateVector(Vector * Scale);
}

FVector FTransform::InverseTransformPosition(const FVector& Position) const noexcept
{
	return ComponentDivideSafe(Rotation.UnrotateVector(Position - Translation), Scale);
}

FVector FTransform::InverseTransformVector(const FVector& Vector) const noexcept
{
	return ComponentDivideSafe(Rotation.UnrotateVector(Vector), Scale);
}

FMatrix FTransform::ToMatrix() const noexcept
{
	return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), Scale);
}

FTransform FTransform::Inverse() const noexcept
{
	const FVector InverseScale = ComponentDivideSafe(FVector::OneVector, Scale);
	const FQuat InverseRotation = Rotation.Inverse();
	const FVector InverseTranslation = InverseRotation.RotateVector(-Translation * InverseScale);
	return FTransform(InverseRotation, InverseTranslation, InverseScale);
}
