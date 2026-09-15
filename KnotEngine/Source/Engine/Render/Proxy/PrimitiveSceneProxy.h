#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"

class FStaticMesh;
class FScene;
class UPrimitiveComponent;
class UStaticMeshComponent;

// Scene이 소유하는 렌더 상태. 등록 기간 동안 Component를 참조하고 Dirty일 때만 데이터를 복사한다.
struct ENGINE_API FPrimitiveSceneProxy
{
	virtual ~FPrimitiveSceneProxy() = default;
	FPrimitiveSceneProxy(const FPrimitiveSceneProxy&) = delete;
	FPrimitiveSceneProxy& operator=(const FPrimitiveSceneProxy&) = delete;

	virtual void Update() = 0;

	FMatrix WorldMatrix;
	FAABB LocalBounds;
	FAABB WorldBounds;
	bool bVisible = false;
	bool bDirty = true;

protected:
	explicit FPrimitiveSceneProxy(const UPrimitiveComponent& InComponent);
	void UpdateBounds(const FAABB& InLocalBounds);

private:
	friend class FScene;
	friend class UPrimitiveComponent;

	static constexpr SIZE_T InvalidSceneIndex = static_cast<SIZE_T>(-1);
	SIZE_T SceneIndex = InvalidSceneIndex; // Scene의 밀집 Proxy 배열에서 현재 위치. 외부 식별자로 사용하지 않는다.
	const UPrimitiveComponent& Component; // 비소유. Component 등록 해제 시 이 Proxy를 먼저 제거한다.
};

// FStaticMeshVertex 기반 Static Mesh의 렌더 상태를 보관한다.
struct ENGINE_API FStaticMeshSceneProxy final : FPrimitiveSceneProxy
{
	explicit FStaticMeshSceneProxy(const UStaticMeshComponent& InComponent);
	void Update() override;

	FStaticMesh* Mesh = nullptr; // Component의 UStaticMesh 참조가 Proxy 등록 기간 동안 Asset 수명을 유지한다.

private:
	const UStaticMeshComponent& MeshComponent;
};
