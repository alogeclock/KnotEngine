#pragma once

#include "EngineAPI.h"
#include "Core/Geometry/AABB.h"
#include "Core/Math/Matrix.h"

class FGeometryMesh;
class FScene;
class UPrimitiveComponent;
class UMeshComponent;

// Scene이 소유하는 렌더 상태. 등록 기간 동안 Component를 참조하고 Dirty일 때만 데이터를 복사한다.
struct ENGINE_API FPrimitiveSceneProxy
{
	explicit FPrimitiveSceneProxy(const UMeshComponent& InComponent);
	FPrimitiveSceneProxy(const FPrimitiveSceneProxy&) = delete;
	FPrimitiveSceneProxy& operator=(const FPrimitiveSceneProxy&) = delete;

	void Update();

	FGeometryMesh* Mesh = nullptr;

	FMatrix WorldMatrix;
	FAABB LocalBounds;
	FAABB WorldBounds;
	bool bVisible = false;
	bool bDirty = true;

private:
	friend class FScene;
	friend class UPrimitiveComponent;
	friend class UMeshComponent;

	static constexpr SIZE_T InvalidSceneIndex = static_cast<SIZE_T>(-1);
	SIZE_T SceneIndex = InvalidSceneIndex; // Scene의 밀집 Proxy 배열에서 현재 위치. 외부 식별자로 사용하지 않는다.
	const UMeshComponent& Component; // 비소유. Component 등록 해제 시 이 Proxy를 먼저 제거한다.
};
