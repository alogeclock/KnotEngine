#include "Core/Geometry/Edge.h"

namespace
{
	size_t CombineHash(size_t Seed, size_t Value) noexcept
	{
		return Seed ^ (Value * 2654435761u + 0x9e3779b9u + (Seed << 6) + (Seed >> 2));
	}
} // namespace

bool FEdge::operator==(const FEdge& Other) const noexcept
{
	return (A == Other.A && B == Other.B) || (A == Other.B && B == Other.A);
}

bool FEdge::operator!=(const FEdge& Other) const noexcept
{
	return !(*this == Other);
}

FVector FEdge::Midpoint() const noexcept
{
	return (A + B) * 0.5f;
}

float FEdge::Length() const noexcept
{
	return FVector::Dist(A, B);
}

float FEdge::LengthSquared() const noexcept
{
	return FVector::DistSquared(A, B);
}

FEdge FEdge::Canonical() const noexcept
{
	const bool bALess = A.X != B.X ? A.X < B.X : (A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z);
	return bALess ? FEdge(A, B) : FEdge(B, A);
}

bool FIndexEdge::operator==(const FIndexEdge& Other) const noexcept
{
	return (A == Other.A && B == Other.B) || (A == Other.B && B == Other.A);
}

bool FIndexEdge::operator!=(const FIndexEdge& Other) const noexcept
{
	return !(*this == Other);
}

FIndexEdge FIndexEdge::Canonical() const noexcept
{
	return A < B ? FIndexEdge(A, B) : FIndexEdge(B, A);
}

size_t std::hash<FEdge>::operator()(const FEdge& Edge) const noexcept
{
	const FEdge C = Edge.Canonical();
	return CombineHash(std::hash<FVector>{}(C.A), std::hash<FVector>{}(C.B));
}

size_t std::hash<FIndexEdge>::operator()(const FIndexEdge& Edge) const noexcept
{
	const FIndexEdge C = Edge.Canonical();
	return CombineHash(std::hash<uint32>{}(C.A), std::hash<uint32>{}(C.B));
}
