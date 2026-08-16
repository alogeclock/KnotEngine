#include "Core/Math/Row.h"

float& FRow::operator[](const int32& Column) noexcept
{
	check(Column >= 0 && Column < 4);
	return Values[Column];
}

const float& FRow::operator[](const int32& Column) const noexcept
{
	check(Column >= 0 && Column < 4);
	return Values[Column];
}

FRow& FRow::operator=(const FRow& Row) noexcept
{
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Values[Index] = Row.Values[Index];
	}
	return *this;
}

FRow& FRow::operator=(const FConstRow& Row) noexcept
{
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Values[Index] = Row.Values[Index];
	}
	return *this;
}

FRow& FRow::operator=(const FVector4& Row) noexcept
{
	Values[0] = Row.X;
	Values[1] = Row.Y;
	Values[2] = Row.Z;
	Values[3] = Row.W;
	return *this;
}

FRow& FRow::operator=(const Float4& Row) noexcept
{
	Values[0] = Row.x;
	Values[1] = Row.y;
	Values[2] = Row.z;
	Values[3] = Row.w;
	return *this;
}

FRow& FRow::operator=(std::initializer_list<float> Row) noexcept
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

FRow::operator FVector4() const noexcept
{
	return FVector4(Values[0], Values[1], Values[2], Values[3]);
}

FRow::operator float*() noexcept
{
	return Values;
}

FRow::operator const float*() const noexcept
{
	return Values;
}

FRow& FRow::operator+=(const FVector4& Row) noexcept
{
	Values[0] += Row.X;
	Values[1] += Row.Y;
	Values[2] += Row.Z;
	Values[3] += Row.W;
	return *this;
}

FRow& FRow::operator-=(const FVector4& Row) noexcept
{
	Values[0] -= Row.X;
	Values[1] -= Row.Y;
	Values[2] -= Row.Z;
	Values[3] -= Row.W;
	return *this;
}

FRow& FRow::operator*=(float Scalar) noexcept
{
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Values[Index] *= Scalar;
	}
	return *this;
}

FRow& FRow::operator/=(float Scalar) noexcept
{
	check(Scalar != 0.f);
	return *this *= 1.0f / Scalar;
}

FVector4 FRow::operator+(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] + Row.X, Values[1] + Row.Y, Values[2] + Row.Z, Values[3] + Row.W);
}

FVector4 FRow::operator-(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] - Row.X, Values[1] - Row.Y, Values[2] - Row.Z, Values[3] - Row.W);
}

FVector4 FRow::operator*(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] * Row.X, Values[1] * Row.Y, Values[2] * Row.Z, Values[3] * Row.W);
}

FVector4 FRow::operator/(const FVector4& Row) const noexcept
{
	check(Row.X != 0.f && Row.Y != 0.f && Row.Z != 0.f && Row.W != 0.f);
	return FVector4(Values[0] / Row.X, Values[1] / Row.Y, Values[2] / Row.Z, Values[3] / Row.W);
}

FVector4 FRow::operator*(float Scalar) const noexcept
{
	return FVector4(Values[0] * Scalar, Values[1] * Scalar, Values[2] * Scalar, Values[3] * Scalar);
}

FVector4 FRow::operator/(float Scalar) const noexcept
{
	check(Scalar != 0.f);
	return *this * (1.0f / Scalar);
}

const float& FConstRow::operator[](int32 Column) const noexcept
{
	check(Column >= 0 && Column < 4);
	return Values[Column];
}

FConstRow::operator FVector4() const noexcept
{
	return FVector4(Values[0], Values[1], Values[2], Values[3]);
}

FConstRow::operator const float*() const noexcept
{
	return Values;
}

FVector4 FConstRow::operator+(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] + Row.X, Values[1] + Row.Y, Values[2] + Row.Z, Values[3] + Row.W);
}

FVector4 FConstRow::operator-(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] - Row.X, Values[1] - Row.Y, Values[2] - Row.Z, Values[3] - Row.W);
}

FVector4 FConstRow::operator*(const FVector4& Row) const noexcept
{
	return FVector4(Values[0] * Row.X, Values[1] * Row.Y, Values[2] * Row.Z, Values[3] * Row.W);
}

FVector4 FConstRow::operator/(const FVector4& Row) const noexcept
{
	check(Row.X != 0.f && Row.Y != 0.f && Row.Z != 0.f && Row.W != 0.f);
	return FVector4(Values[0] / Row.X, Values[1] / Row.Y, Values[2] / Row.Z, Values[3] / Row.W);
}

FVector4 FConstRow::operator*(float Scalar) const noexcept
{
	return FVector4(Values[0] * Scalar, Values[1] * Scalar, Values[2] * Scalar, Values[3] * Scalar);
}

FVector4 FConstRow::operator/(float Scalar) const noexcept
{
	check(Scalar != 0.f);
	return *this * (1.0f / Scalar);
}

FVector4 operator*(float Scalar, const FRow& Row) noexcept
{
	return Row * Scalar;
}

FVector4 operator*(float Scalar, const FConstRow& Row) noexcept
{
	return Row * Scalar;
}