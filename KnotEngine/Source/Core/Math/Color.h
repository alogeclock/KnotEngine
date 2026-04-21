#pragma once

#include "Core/CoreTypes.h"
#include "Math/Utils.h"
#include "Math/Vector4.h"

struct FColor
{
public:
	union
	{
		struct { float r, g, b, a; };
		struct { float R, G, B, A; };
	};

	constexpr FColor() noexcept : r(0.f), g(0.f), b(0.f), a(1.f) {}

	constexpr FColor(float InR, float InG, float InB, float InA) noexcept
		: r(InR), g(InG), b(InB), a(InA) {}

	constexpr FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA = 255) noexcept
		: r(static_cast<float>(InR) / 255.0f)
		, g(static_cast<float>(InG) / 255.0f)
		, b(static_cast<float>(InB) / 255.0f)
		, a(static_cast<float>(InA) / 255.0f)
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
		return { Clamp(r + Num), Clamp(g + Num), Clamp(b + Num), a };
	}

	constexpr FColor operator+(const FColor& Other) const noexcept
	{
		return { Clamp(r + Other.r), Clamp(g + Other.g), Clamp(b + Other.b), Clamp(a + Other.a) };
	}

	constexpr FColor operator-(float Num) const noexcept
	{
		return { Clamp(r - Num), Clamp(g - Num), Clamp(b - Num), a };
	}

	constexpr FColor operator-(const FColor& Other) const noexcept
	{
		return { Clamp(r - Other.r), Clamp(g - Other.g), Clamp(b - Other.b), Clamp(a - Other.a) };
	}

	constexpr FColor operator*(float Num) const noexcept
	{
		return { Clamp(r * Num), Clamp(g * Num), Clamp(b * Num), a };
	}

	constexpr FColor operator*(const FColor& Other) const noexcept
	{
		return { Clamp(r * Other.r), Clamp(g * Other.g), Clamp(b * Other.b), Clamp(a * Other.a) };
	}

	// ──────────── Methods ────────────
public:
	constexpr FVector4 ToVector4() const noexcept { return FVector4(r, g, b, a); }

	constexpr uint32 ToPackedABGR() const noexcept
	{
		uint32 Ri = static_cast<uint32>(r * 255.999f);
		uint32 Gi = static_cast<uint32>(g * 255.999f);
		uint32 Bi = static_cast<uint32>(b * 255.999f);
		uint32 Ai = static_cast<uint32>(a * 255.999f);
		return (Ai << 24) | (Bi << 16) | (Gi << 8) | Ri;
	}

	static constexpr FColor Lerp(const FColor& A, const FColor& B, float T) noexcept
	{
		return {
			Clamp(A.r + (B.r - A.r) * T),
			Clamp(A.g + (B.g - A.g) * T),
			Clamp(A.b + (B.b - A.b) * T),
			Clamp(A.a + (B.a - A.a) * T)
		};
	}

private:
	static constexpr float Clamp(float Value) noexcept
	{
		return (Value < 0.0f) ? 0.0f : ((Value > 1.0f) ? 1.0f : Value);
	}
};
