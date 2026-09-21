#pragma once

#include "EngineAPI.h"
#include "Asset/Asset/AssetId.h"
#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"
#include "Render/Scene/RenderCommand.h"

class FStaticMeshResource;
class FMaterialResource;
struct FSceneView;
class FScene;
class FRenderer;

// Component가 Game Thread에서 추출해 제출하는 값 복사본이다. Render Thread는 Asset UObject를 조회하지 않는다.
struct ENGINE_API FPrimitiveRenderData
{
	FMatrix WorldMatrix;
	FAABB LocalBounds;

	FAssetId MeshAssetId;
	TArray<FAssetId> MaterialAssetIds;

	bool bVisible = false;
	bool bSelected = false;
	bool bLODEnable = true;
};

// Render Thread의 Scene이 소유하며 UObject나 Component를 직접 조회하지 않는 렌더 상태다.
struct ENGINE_API FPrimitiveSceneProxy
{
	virtual ~FPrimitiveSceneProxy() = default;
	FPrimitiveSceneProxy(const FPrimitiveSceneProxy&) = delete;
	FPrimitiveSceneProxy& operator=(const FPrimitiveSceneProxy&) = delete;

	virtual void Apply(ERenderCommandType Type, const FPrimitiveRenderData& RenderData, FRenderer& Renderer) = 0;

	FMatrix WorldMatrix;
	FAABB LocalBounds;
	FAABB WorldBounds;
	float WorldBoundsRadius = 0.0f;
	bool bVisible = false;
	bool bSelected = false; // 선택 변경 시 Component가 갱신하며 렌더 경로는 UObject Owner를 조회하지 않는다.

protected:
	FPrimitiveSceneProxy() = default;

	void ApplyPrimitiveData(ERenderCommandType Type, const FPrimitiveRenderData& RenderData);

private:
	friend class FScene;

	static constexpr SIZE_T InvalidSceneIndex = static_cast<SIZE_T>(-1);
	SIZE_T SceneIndex = InvalidSceneIndex; // Scene의 밀집 Proxy 배열에서 현재 위치. 외부 식별자로 사용하지 않는다.
};

// FStaticMeshVertex 기반 Static Mesh의 렌더 상태를 보관한다.
struct ENGINE_API FStaticMeshSceneProxy final : FPrimitiveSceneProxy
{
	explicit FStaticMeshSceneProxy(const FPrimitiveRenderData& RenderData);
	void Apply(ERenderCommandType Type, const FPrimitiveRenderData& RenderData, FRenderer& Renderer) override;

	FStaticMeshResource* MeshResource = nullptr; // Renderer의 Static Mesh Resource Cache가 소유한다.
	TArray<const FMaterialResource*> Materials;
	const FMaterialResource& GetMaterial(SIZE_T MaterialIndex) const;
	SIZE_T SelectLOD(const FSceneView& View) const;
	
	bool bLODEnable = true;
	const FMaterialResource* DefaultMaterial = nullptr;

};
