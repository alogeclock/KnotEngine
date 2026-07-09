#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Utils.h"
#include "Core/Math/Vector4.h"

struct FColor
{
public:
	union
	{
		struct { uint8 r, g, b, a; };
		struct { uint8 R, G, B, A; };
	};

	constexpr FColor() noexcept : r(0), g(0), b(0), a(255) {}

	constexpr FColor(float InR, float InG, float InB, float InA) noexcept
		: r(FloatToByte(InR)), g(FloatToByte(InG)), b(FloatToByte(InB)), a(FloatToByte(InA)) {}

	constexpr FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA = 255) noexcept
		: r(ClampByte(InR))
		, g(ClampByte(InG))
		, b(ClampByte(InB))
		, a(ClampByte(InA))
	{
	}

	~FColor() = default;

	// ──────────── preset colors ────────────
public:
	static constexpr FColor White()       { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
	static constexpr FColor Black()       { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
	static constexpr FColor Red()         { return { 1.0f, 0.0f, 0.0f, 1.0f }; }
	static constexpr FColor Green()       { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
	static constexpr FColor Blue()        { return { 0.0f, 0.0f, 1.0f, 1.0f }; }
	static constexpr FColor Yellow()      { return { 1.0f, 1.0f, 0.0f, 1.0f }; }
	static constexpr FColor Magenta()     { return { 1.0f, 0.0f, 1.0f, 1.0f }; }
	static constexpr FColor Cyan()        { return { 0.0f, 1.0f, 1.0f, 1.0f }; }
	static constexpr FColor Gray()        { return { 0.5f, 0.5f, 0.5f, 1.0f }; }
	static constexpr FColor Transparent() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }

	// ──────────── Operators ────────────
public:
	constexpr FColor operator+(float Num) const noexcept
	{
		return { Clamp(ByteToFloat(r) + Num), Clamp(ByteToFloat(g) + Num), Clamp(ByteToFloat(b) + Num), ByteToFloat(a) };
	}

	constexpr FColor operator+(const FColor& Other) const noexcept
	{
		return {
			Clamp(ByteToFloat(r) + ByteToFloat(Other.r)),
			Clamp(ByteToFloat(g) + ByteToFloat(Other.g)),
			Clamp(ByteToFloat(b) + ByteToFloat(Other.b)),
			Clamp(ByteToFloat(a) + ByteToFloat(Other.a))
		};
	}

	constexpr FColor operator-(float Num) const noexcept
	{
		return { Clamp(ByteToFloat(r) - Num), Clamp(ByteToFloat(g) - Num), Clamp(ByteToFloat(b) - Num), ByteToFloat(a) };
	}

	constexpr FColor operator-(const FColor& Other) const noexcept
	{
		return {
			Clamp(ByteToFloat(r) - ByteToFloat(Other.r)),
			Clamp(ByteToFloat(g) - ByteToFloat(Other.g)),
			Clamp(ByteToFloat(b) - ByteToFloat(Other.b)),
			Clamp(ByteToFloat(a) - ByteToFloat(Other.a))
		};
	}

	constexpr FColor operator*(float Num) const noexcept
	{
		return { Clamp(ByteToFloat(r) * Num), Clamp(ByteToFloat(g) * Num), Clamp(ByteToFloat(b) * Num), ByteToFloat(a) };
	}

	constexpr FColor operator*(const FColor& Other) const noexcept
	{
		return {
			Clamp(ByteToFloat(r) * ByteToFloat(Other.r)),
			Clamp(ByteToFloat(g) * ByteToFloat(Other.g)),
			Clamp(ByteToFloat(b) * ByteToFloat(Other.b)),
			Clamp(ByteToFloat(a) * ByteToFloat(Other.a))
		};
	}

	// ──────────── Methods ────────────
public:
	constexpr FVector4 ToVector4() const noexcept { return FVector4(ByteToFloat(r), ByteToFloat(g), ByteToFloat(b), ByteToFloat(a)); }

	constexpr uint32 ToPackedABGR() const noexcept
	{
		return (static_cast<uint32>(a) << 24) | (static_cast<uint32>(b) << 16) | (static_cast<uint32>(g) << 8) | static_cast<uint32>(r);
	}

	static constexpr FColor Lerp(const FColor& A, const FColor& B, float T) noexcept
	{
		return {
			Clamp(ByteToFloat(A.r) + (ByteToFloat(B.r) - ByteToFloat(A.r)) * T),
			Clamp(ByteToFloat(A.g) + (ByteToFloat(B.g) - ByteToFloat(A.g)) * T),
			Clamp(ByteToFloat(A.b) + (ByteToFloat(B.b) - ByteToFloat(A.b)) * T),
			Clamp(ByteToFloat(A.a) + (ByteToFloat(B.a) - ByteToFloat(A.a)) * T)
		};
	}

private:
	static constexpr float ByteToFloat(uint8 Value) noexcept
	{
		return static_cast<float>(Value) / 255.0f;
	}

	static constexpr uint8 FloatToByte(float Value) noexcept
	{
		return static_cast<uint8>(Clamp(Value) * 255.999f);
	}

	static constexpr uint8 ClampByte(uint32 Value) noexcept
	{
		return static_cast<uint8>((Value > 255) ? 255 : Value);
	}

	static constexpr float Clamp(float Value) noexcept
	{
		return (Value < 0.0f) ? 0.0f : ((Value > 1.0f) ? 1.0f : Value);
	}
};

static_assert(sizeof(FColor) == 4, "FColor must be 4 bytes.");
