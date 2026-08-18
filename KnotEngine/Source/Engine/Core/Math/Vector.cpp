#include "Core/Math/Vector.h"

#include <cmath>

const FVector FVector::ZeroVector{ 0.0f, 0.0f, 0.0f };
const FVector FVector::OneVector{ 1.0f, 1.0f, 1.0f };
const FVector FVector::ForwardVector{ 1.0f, 0.0f, 0.0f };
const FVector FVector::RightVector{ 0.0f, 1.0f, 0.0f };
const FVector FVector::UpVector{ 0.0f, 0.0f, 1.0f };

float FVector::Size() const noexcept
{
	return std::sqrt(SizeSquared());
}

bool FVector::Normalize(float Tolerance) noexcept
{
	const float SquaredSize = SizeSquared();
	if (SquaredSize <= Tolerance)
	{
		*this = ZeroVector;
		return false;
	}

	*this *= 1.0f / std::sqrt(SquaredSize);
	return true;
}

FVector FVector::GetSafeNormal(float Tolerance) const noexcept
{
	FVector Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

float FVector::Dist(const FVector& A, const FVector& B) noexcept
{
	return (A - B).Size();
}

size_t std::hash<FVector>::operator()(const FVector& Vector) const noexcept
{
	size_t Hash = std::hash<float>{}(Vector.X);
	Hash ^= std::hash<float>{}(Vector.Y) * 2654435761u + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
	Hash ^= std::hash<float>{}(Vector.Z) * 2654435761u + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
	return Hash;
}
