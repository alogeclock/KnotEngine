#pragma once

#include "EngineAPI.h"

// GPU Handle을 직접 소유하는 리소스의 공통 생명주기 계약.
// 현재 Texture Resource와 Mesh Buffer가 상속하며 Release 후 초기화 상태를 일관되게 관리한다.
// FStaticMeshResource 등 캐시 단위 집합은 이를 상속하지 않고 FRenderResource들을 소유한다.	
class ENGINE_API FRenderResource
{
public:
	FRenderResource() = default;
	virtual ~FRenderResource() = default;

	FRenderResource(const FRenderResource&) = delete;
	FRenderResource& operator=(const FRenderResource&) = delete;
	FRenderResource(FRenderResource&&) = delete;
	FRenderResource& operator=(FRenderResource&&) = delete;

	void Release();

	bool IsInitialized() const { return bInitialized; }

protected:
	virtual void OnRelease() = 0;
	bool bInitialized = false;
};
