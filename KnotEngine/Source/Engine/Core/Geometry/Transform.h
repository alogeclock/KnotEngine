#pragma once

#include "Core/Math/Matrix.h"
#include "Core/Math/Quat.h"
#include "Core/Math/Rotator.h"

struct FTransform
{
public:
	static const FTransform Identity;

	FTransform() noexcept = default;

	explicit FTransform(const FQuat& InRotation) noexcept
		: Rotation(InRotation.GetNormalized())
	{
	}

	explicit FTransform(const FRotator& InRotation) noexcept
		: Rotation(InRotation.Quaternion())
	{
	}

	FTransform(const FQuat& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector) noexcept
		: Rotation(InRotation.GetNormalized())
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	FTransform(const FRotator& InRotation, const FVector& InTranslation, const FVector& InScale3D = FVector::OneVector) noexcept
		: Rotation(InRotation.Quaternion())
		, Translation(InTranslation)
		, Scale3D(InScale3D)
	{
	}

	explicit FTransform(const FMatrix& InMatrix) noexcept
		: Rotation(FQuat::Identity)
		, Translation(FVector::ZeroVector)
		, Scale3D(FVector::OneVector)
	{
		DirectX::XMVECTOR OutScale;
		DirectX::XMVECTOR OutRotation;
		DirectX::XMVECTOR OutTranslation;

		if (DirectX::XMMatrixDecompose(&OutScale, &OutRotation, &OutTranslation, InMatrix.ToXMMatrix()))
		{
			Scale3D = FVector(OutScale);
			Rotation = FQuat(OutRotation).GetNormalized();
			Translation = FVector(OutTranslation);
		}
	}

	const FVector& GetLocation() const noexcept { return Translation; }
	const FVector& GetTranslation() const noexcept { return Translation; }
	const FQuat& GetRotation() const noexcept { return Rotation; }
	const FVector& GetScale3D() const noexcept { return Scale3D; }

	void SetLocation(const FVector& InTranslation) noexcept { Translation = InTranslation; }
	void SetTranslation(const FVector& InTranslation) noexcept { Translation = InTranslation; }
	void SetRotation(const FQuat& InRotation) noexcept { Rotation = InRotation.GetNormalized(); }
	void SetRotation(const FRotator& InRotation) noexcept { Rotation = InRotation.Quaternion(); }
	void SetScale3D(const FVector& InScale3D) noexcept { Scale3D = InScale3D; }
	void SetIdentity() noexcept
	{
		Rotation = FQuat::Identity;
		Translation = FVector::ZeroVector;
		Scale3D = FVector::OneVector;
	}

	FRotator Rotator() const noexcept { return Rotation.Rotator(); }
	void NormalizeRotation() noexcept { Rotation.Normalize(); }
	bool Equals(const FTransform& Other, float Tolerance = 1.e-6f) const noexcept
	{
		return Translation.Equals(Other.Translation, Tolerance)
			&& Rotation.Equals(Other.Rotation, Tolerance)
			&& Scale3D.Equals(Other.Scale3D, Tolerance);
	}

	bool IsIdentity(float Tolerance = 1.e-6f) const noexcept { return Equals(Identity, Tolerance); }
	void AddToTranslation(const FVector& DeltaTranslation) noexcept { Translation += DeltaTranslation; }

	FVector TransformPosition(const FVector& InPosition) const noexcept
	{
		return Rotation.RotateVector(ComponentMultiply(InPosition, Scale3D)) + Translation;
	}

	FVector TransformPositionNoScale(const FVector& InPosition) const noexcept
	{
		return Rotation.RotateVector(InPosition) + Translation;
	}

	FVector TransformVector(const FVector& InVector) const noexcept
	{
		return Rotation.RotateVector(ComponentMultiply(InVector, Scale3D));
	}

	FVector TransformVectorNoScale(const FVector& InVector) const noexcept
	{
		return Rotation.RotateVector(InVector);
	}

	FVector InverseTransformPosition(const FVector& InPosition) const noexcept
	{
		const FVector Untranslated = InPosition - Translation;
		const FVector Unrotated = Rotation.UnrotateVector(Untranslated);
		return ComponentDivideSafe(Unrotated, Scale3D);
	}

	FVector InverseTransformPositionNoScale(const FVector& InPosition) const noexcept
	{
		return Rotation.UnrotateVector(InPosition - Translation);
	}

	FVector InverseTransformVector(const FVector& InVector) const noexcept
	{
		return ComponentDivideSafe(Rotation.UnrotateVector(InVector), Scale3D);
	}

	FVector InverseTransformVectorNoScale(const FVector& InVector) const noexcept
	{
		return Rotation.UnrotateVector(InVector);
	}

	FVector GetUnitAxis(EAxis Axis) const noexcept { return GetScaledAxis(Axis).GetSafeNormal(); }

	FVector GetScaledAxis(EAxis Axis) const noexcept
	{
		switch (Axis)
		{
		case EAxis::X:
			return Rotation.RotateVector(FVector(Scale3D.X, 0.0f, 0.0f));
		case EAxis::Y:
			return Rotation.RotateVector(FVector(0.0f, Scale3D.Y, 0.0f));
		case EAxis::Z:
			return Rotation.RotateVector(FVector(0.0f, 0.0f, Scale3D.Z));
		default:
			return FVector::ZeroVector;
		}
	}

	FMatrix ToMatrixNoScale() const noexcept
	{
		return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), FVector::OneVector);
	}

	FMatrix ToMatrixWithScale() const noexcept
	{
		return FMatrix::MakeWorld(Translation, Rotation.ToMatrix(), Scale3D);
	}

	FMatrix ToInverseMatrixWithScale() const noexcept
	{
		return ToMatrixWithScale().GetInverse();
	}

	FMatrix ToMatrix() const noexcept
	{
		return ToMatrixWithScale();
	}

	FTransform Inverse() const noexcept
	{
		const FVector InverseScale3D = GetSafeScaleReciprocal(Scale3D);
		const FQuat InverseRotation = Rotation.Inverse();
		const FVector InverseTranslation =
			InverseRotation.RotateVector(ComponentMultiply(-Translation, InverseScale3D));

		return FTransform(InverseRotation, InverseTranslation, InverseScale3D);
	}

	FTransform operator*(const FTransform& Other) const noexcept
	{
		const FVector ResultScale3D = ComponentMultiply(Scale3D, Other.Scale3D);
		const FQuat ResultRotation = Rotation * Other.Rotation;
		const FVector ResultTranslation = Other.TransformPosition(Translation);

		return FTransform(ResultRotation, ResultTranslation, ResultScale3D);
	}

	FTransform& operator*=(const FTransform& Other) noexcept
	{
		*this = *this * Other;
		return *this;
	}

private:
	static FVector ComponentMultiply(const FVector& A, const FVector& B) noexcept
	{
		return FVector(DirectX::XMVectorMultiply(A.ToXMVector(), B.ToXMVector()));
	}

	static FVector ComponentDivideSafe(const FVector& A, const FVector& B, float Tolerance = 1.e-8f) noexcept
	{
		const XMVector Numerator = A.ToXMVector();
		const XMVector Denominator = B.ToXMVector();
		const XMVector SafeMask = DirectX::XMVectorGreater(
			DirectX::XMVectorAbs(Denominator),
			DirectX::XMVectorReplicate(Tolerance));
		const XMVector Quotient = DirectX::XMVectorDivide(Numerator, Denominator);
		return FVector(DirectX::XMVectorSelect(DirectX::XMVectorZero(), Quotient, SafeMask));
	}

	static FVector GetSafeScaleReciprocal(const FVector& InScale, float Tolerance = 1.e-8f) noexcept
	{
		return ComponentDivideSafe(FVector::OneVector, InScale, Tolerance);
	}

private:
	FQuat Rotation = FQuat::Identity;
	FVector Translation = FVector::ZeroVector;
	FVector Scale3D = FVector::OneVector;
};

inline const FTransform FTransform::Identity(FQuat::Identity, FVector::ZeroVector, FVector::OneVector);
