#include "Core/Math/Vector2.h"

#include <cmath>

const FVector2 FVector2::ZeroVector{ 0.0f, 0.0f };
const FVector2 FVector2::OneVector{ 1.0f, 1.0f };

float FVector2::Size() const noexcept
{
	return std::sqrt(SizeSquared());
}

bool FVector2::Normalize(float Tolerance) noexcept
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

FVector2 FVector2::GetSafeNormal(float Tolerance) const noexcept
{
	FVector2 Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}

float FVector2::Dist(const FVector2& A, const FVector2& B) noexcept
{
	return (A - B).Size();
}

size_t std::hash<FVector2>::operator()(const FVector2& Vector) const noexcept
{
	const size_t HashX = std::hash<float>{}(Vector.X);
	const size_t HashY = std::hash<float>{}(Vector.Y);
	return HashX ^ (HashY * 2654435761u + 0x9e3779b9u + (HashX << 6) + (HashX >> 2));
}
