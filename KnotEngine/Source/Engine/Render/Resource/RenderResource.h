#pragma once

// Renderer가 사용하는 GPU 리소스의 공통 생명주기 계약.
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
