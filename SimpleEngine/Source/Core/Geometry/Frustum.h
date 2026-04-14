#pragma once

#include "Engine/Core/CoreTypes.h"
#include "Core/Geometry/Plane.h"
#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"

struct FFrustum
{
    FPlane Planes[6];

    enum class EFrustumIntersectResult
    {
        Outside,
        Intersect,
        Inside
    };

    void UpdateFromCamera(const FMatrix& View, const FMatrix& Projection);
    void UpdateFromCamera(const FMatrix& ViewProjection);

    EFrustumIntersectResult Intersects(const FAABB& Box) const;
    bool Contains(const FVector& Point) const;
};
