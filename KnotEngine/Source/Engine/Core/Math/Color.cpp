#include "Core/Math/Color.h"

FColor::FColor() noexcept
	: r(0), g(0), b(0), a(255) {}

FColor::FColor(float InR, float InG, float InB, float InA) noexcept
	: r(FloatToByte(InR)), g(FloatToByte(InG)), b(FloatToByte(InB)), a(FloatToByte(InA))
{
}

FColor::FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA) noexcept
	: r(ClampByte(InR)), g(ClampByte(InG)), b(ClampByte(InB)), a(ClampByte(InA))
{
}

FColor FColor::operator+(float Num) const noexcept
{
	return FColor(Clamp(ByteToFloat(r) + Num), Clamp(ByteToFloat(g) + Num), Clamp(ByteToFloat(b) + Num), ByteToFloat(a));
}

FColor FColor::operator+(const FColor& Other) const noexcept
{
	return FColor(
		Clamp(ByteToFloat(r) + ByteToFloat(Other.r)),
		Clamp(ByteToFloat(g) + ByteToFloat(Other.g)),
		Clamp(ByteToFloat(b) + ByteToFloat(Other.b)),
		Clamp(ByteToFloat(a) + ByteToFloat(Other.a)));
}

FColor FColor::operator-(float Num) const noexcept
{
	return FColor(Clamp(ByteToFloat(r) - Num), Clamp(ByteToFloat(g) - Num), Clamp(ByteToFloat(b) - Num), ByteToFloat(a));
}

FColor FColor::operator-(const FColor& Other) const noexcept
{
	return FColor(
		Clamp(ByteToFloat(r) - ByteToFloat(Other.r)),
		Clamp(ByteToFloat(g) - ByteToFloat(Other.g)),
		Clamp(ByteToFloat(b) - ByteToFloat(Other.b)),
		Clamp(ByteToFloat(a) - ByteToFloat(Other.a)));
}

FColor FColor::operator*(float Num) const noexcept
{
	return FColor(Clamp(ByteToFloat(r) * Num), Clamp(ByteToFloat(g) * Num), Clamp(ByteToFloat(b) * Num), ByteToFloat(a));
}

FColor FColor::operator*(const FColor& Other) const noexcept
{
	return FColor(
		Clamp(ByteToFloat(r) * ByteToFloat(Other.r)),
		Clamp(ByteToFloat(g) * ByteToFloat(Other.g)),
		Clamp(ByteToFloat(b) * ByteToFloat(Other.b)),
		Clamp(ByteToFloat(a) * ByteToFloat(Other.a)));
}

FVector4 FColor::ToVector4() const noexcept
{
	return FVector4(ByteToFloat(r), ByteToFloat(g), ByteToFloat(b), ByteToFloat(a));
}

uint32 FColor::ToPackedABGR() const noexcept
{
	return (static_cast<uint32>(a) << 24) | (static_cast<uint32>(b) << 16) | (static_cast<uint32>(g) << 8) | static_cast<uint32>(r);
}

FColor FColor::White() noexcept
{
	return FColor(1.0f, 1.0f, 1.0f, 1.0f);
}

FColor FColor::Black() noexcept
{
	return FColor(0.0f, 0.0f, 0.0f, 1.0f);
}

FColor FColor::Red() noexcept
{
	return FColor(1.0f, 0.0f, 0.0f, 1.0f);
}

FColor FColor::Green() noexcept
{
	return FColor(0.0f, 1.0f, 0.0f, 1.0f);
}

FColor FColor::Blue() noexcept
{
	return FColor(0.0f, 0.0f, 1.0f, 1.0f);
}

FColor FColor::Yellow() noexcept
{
	return FColor(1.0f, 1.0f, 0.0f, 1.0f);
}

FColor FColor::Magenta() noexcept
{
	return FColor(1.0f, 0.0f, 1.0f, 1.0f);
}

FColor FColor::Cyan() noexcept
{
	return FColor(0.0f, 1.0f, 1.0f, 1.0f);
}

FColor FColor::Gray() noexcept
{
	return FColor(0.5f, 0.5f, 0.5f, 1.0f);
}

FColor FColor::Transparent() noexcept
{
	return FColor(0.0f, 0.0f, 0.0f, 0.0f);
}

FColor FColor::Lerp(const FColor& A, const FColor& B, float T) noexcept
{
	return FColor(
		Clamp(ByteToFloat(A.r) + (ByteToFloat(B.r) - ByteToFloat(A.r)) * T),
		Clamp(ByteToFloat(A.g) + (ByteToFloat(B.g) - ByteToFloat(A.g)) * T),
		Clamp(ByteToFloat(A.b) + (ByteToFloat(B.b) - ByteToFloat(A.b)) * T),
		Clamp(ByteToFloat(A.a) + (ByteToFloat(B.a) - ByteToFloat(A.a)) * T));
}

float FColor::ByteToFloat(uint8 Value) noexcept
{
	return static_cast<float>(Value) / 255.0f;
}

uint8 FColor::FloatToByte(float Value) noexcept
{
	return static_cast<uint8>(Clamp(Value) * 255.999f);
}

uint8 FColor::ClampByte(uint32 Value) noexcept
{
	return static_cast<uint8>(Value > 255 ? 255 : Value);
}

float FColor::Clamp(float Value) noexcept
{
	return Value < 0.0f ? 0.0f : (Value > 1.0f ? 1.0f : Value);
}
