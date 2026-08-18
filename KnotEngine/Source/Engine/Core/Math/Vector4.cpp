#include "Core/Math/Vector4.h"

#include <cmath>

const FVector4 FVector4::ZeroVector{ 0.0f, 0.0f, 0.0f, 0.0f };

float FVector4::Size() const noexcept
{
	return std::sqrt(SizeSquared());
}

bool FVector4::Normalize(float Tolerance) noexcept
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

FVector4 FVector4::GetSafeNormal(float Tolerance) const noexcept
{
	FVector4 Result = *this;
	Result.Normalize(Tolerance);
	return Result;
}
