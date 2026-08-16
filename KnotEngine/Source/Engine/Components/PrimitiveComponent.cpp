#include "Components/PrimitiveComponent.h"

#include "Core/Math/Matrix.h"
#include "Render/Renderer.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Resource/MeshTypes.h"

void UPrimitiveComponent::Render(float DeltaTime, URenderer& Renderer)
{
    if (!Mesh || !Mesh->GetMeshBuffer())
    {
        return;
    }

    Rotation += DeltaTime;

    const FRenderViewport ViewportInfo = Renderer.GetViewport();
    const float AspectRatio = ViewportInfo.Height > 0.0f ? ViewportInfo.Width / ViewportInfo.Height : 1.0f;

    const FMatrix World = FMatrix::MakeRotationZ(Rotation * 0.25f) * FMatrix::MakeRotationX(Rotation * 0.65f) * FMatrix::MakeRotationY(Rotation);
    const FMatrix View = FMatrix::MakeLookAt(FVector(-5.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector::UpVector);
    const FMatrix Projection = FMatrix::MakePerspectiveFov(KMath::ToRadian(60.0f), AspectRatio, 0.1f, 100.0f);

    Renderer.UpdateConstant(World * View * Projection);
    Renderer.PrepareShader();
    Renderer.DrawMeshBuffer(*Mesh->GetMeshBuffer());
}
