#pragma once

#include "Core/CoreTypes.h"
#include "Core/Math/Vector.h"
#include <functional>

// 두 FVector 정점으로 구성된 간선(Edge)을 표현하는 자료형
// {A, B}와 {B, A}는 동일한 간선으로 취급됨 (비방향 간선)
struct FEdge
{
	// 멤버 변수 (Member Variables)
	FVector A;
	FVector B;

	// 생성자 (Constructors)
	constexpr FEdge() noexcept
	    : A(), B() {}
	constexpr FEdge(const FVector& InA, const FVector& InB) noexcept
	    : A(InA), B(InB) {}
	FEdge(const FEdge&) noexcept = default;
	FEdge(FEdge&&) noexcept = default;

	// 비교 및 대입 연산자 (Comparison and Assignment Operators)
	FEdge& operator=(const FEdge&) noexcept = default;
	FEdge& operator=(FEdge&&) noexcept = default;
	bool operator==(const FEdge& Other) const noexcept;
	bool operator!=(const FEdge& Other) const noexcept;

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
	FVector Midpoint() const noexcept;
	float Length() const noexcept;
	float LengthSquared() const noexcept;
	FEdge Canonical() const noexcept;
};

namespace std
{
	template <>
	struct hash<FEdge>
	{
		size_t operator()(const FEdge& Edge) const noexcept;
	};
} // namespace std

// 두 정점의 인덱스(Index)로 구성된 간선(Edge)을 표현하는 자료형
// (A, B)와 (B, A)는 동일한 간선으로 취급됨 (비방향 간선)
struct FIndexEdge
{
public:
	// 멤버 변수 (Member Variables)
	uint32 A;
	uint32 B;

	// 생성자 (Constructors)
public:
	constexpr FIndexEdge() noexcept
	    : A(0), B(0) {}

	constexpr FIndexEdge(uint32 InA, uint32 InB) noexcept
	    : A(InA), B(InB)
	{
	}

	FIndexEdge(const FIndexEdge&) noexcept = default;
	FIndexEdge(FIndexEdge&&) noexcept = default;

	// 비교 및 대입 연산자 (Comparison and Assignment Operators)
public:
	FIndexEdge& operator=(const FIndexEdge&) noexcept = default;
	FIndexEdge& operator=(FIndexEdge&&) noexcept = default;

	bool operator==(const FIndexEdge& Other) const noexcept;
	bool operator!=(const FIndexEdge& Other) const noexcept;

	// 인스턴스 유틸리티 함수 (Instance Utility Functions)
public:
	FIndexEdge Canonical() const noexcept;
};

namespace std
{
	template <>
	struct hash<FIndexEdge>
	{
		size_t operator()(const FIndexEdge& Edge) const noexcept;
	};
} // namespace std
