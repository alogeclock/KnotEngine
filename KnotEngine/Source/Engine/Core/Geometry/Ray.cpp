
#include "Core/Geometry/Ray.h"

#include "Core/Math/Matrix.h"

// FRay 객체를 초기화합니다.
FRay::FRay(const FVector& InOrigin, const FVector& InDirection)
    : Origin(InOrigin)
{
    SetDirection(InDirection);
}

// Direction 값을 설정합니다.
void FRay::SetDirection(const FVector& NewDirection)
{
    Direction = NewDirection;

    // 0으로 나누면 IEEE 754에 의해 ±infinity가 되어 slab 테스트에서 평행 케이스를 자동 처리
    InvD.X = 1.0f / Direction.X;
    InvD.Y = 1.0f / Direction.Y;
    InvD.Z = 1.0f / Direction.Z;
}

// 입력값으로 BuildRay 결과를 생성합니다.
FRay FRay::BuildRay(int32 MouseX, int32 MouseY, const FMatrix& ViewProjection, float ViewportWidth, float ViewportHeight)
{
    if (ViewportWidth <= 0 || ViewportHeight <= 0)
    {
        return FRay{};
    }

    const float NDCX = 2.0f * static_cast<float>(MouseX) / ViewportWidth - 1.0f;
    const float NDCY = 1.0f - 2.0f * static_cast<float>(MouseY) / ViewportHeight;
    const FVector NearPointNDC(NDCX, NDCY, 0.0f);
    const FVector FarPointNDC(NDCX, NDCY, 1.0f);
    const FMatrix InvViewProjection = ViewProjection.GetInverse();
    const FVector NearWorld = InvViewProjection.TransformPosition(NearPointNDC);
    const FVector FarWorld = InvViewProjection.TransformPosition(FarPointNDC);
    const FVector Direction = (FarWorld - NearWorld).GetSafeNormal();

    return FRay{ NearWorld, Direction };
}
