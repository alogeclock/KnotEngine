#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Vector4.h"

struct FColor
{
	// 멤버 변수 (Member Variables)
	union
	{
		struct { uint8 r, g, b, a; };
		struct { uint8 R, G, B, A; };
	};

	// 생성자 (Constructors)
	FColor() noexcept;
	FColor(float InR, float InG, float InB, float InA) noexcept;
	FColor(uint32 InR, uint32 InG, uint32 InB, uint32 InA = 255) noexcept;
	~FColor() = default;

	// 일반 사칙 연산자 (Basic Math Operators)
	FColor operator+(float Num) const noexcept;
	FColor operator+(const FColor& Other) const noexcept;
	FColor operator-(float Num) const noexcept;
	FColor operator-(const FColor& Other) const noexcept;
	FColor operator*(float Num) const noexcept;
	FColor operator*(const FColor& Other) const noexcept;

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	FVector4 ToVector4() const noexcept;
	uint32 ToPackedABGR() const noexcept;

	// 공용 색상 생성 함수 (Static Color Functions)
	static FColor White() noexcept;
	static FColor Black() noexcept;
	static FColor Red() noexcept;
	static FColor Green() noexcept;
	static FColor Blue() noexcept;
	static FColor Yellow() noexcept;
	static FColor Magenta() noexcept;
	static FColor Cyan() noexcept;
	static FColor Gray() noexcept;
	static FColor Transparent() noexcept;
	static FColor Lerp(const FColor& A, const FColor& B, float T) noexcept;

private:
	// 내부 헬퍼 함수 (Private Helper Functions)
	static float ByteToFloat(uint8 Value) noexcept;
	static uint8 FloatToByte(float Value) noexcept;
	static uint8 ClampByte(uint32 Value) noexcept;
	static float Clamp(float Value) noexcept;
};

static_assert(sizeof(FColor) == 4, "FColor must be 4 bytes.");
