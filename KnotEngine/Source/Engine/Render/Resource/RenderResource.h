#pragma once

// Renderer가 사용하는 GPU 리소스의 공통 생명주기 계약.
// 버퍼, 텍스처, 렌더 타깃, 메시 및 머티리얼의 GPU 표현 등이 이를 상속한다.
class FRenderResource
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
