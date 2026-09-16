#pragma once

#include "EngineAPI.h"

#include "Render/Resource/RenderResource.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

class IRenderDevice;

// Texture UObject와 독립적으로 GPU Texture Handle의 생성 및 해제를 관리하는 렌더 리소스다.
class ENGINE_API FTexture final : public FRenderResource
{
public:
	FTexture() = default;
	~FTexture() override;
	FTexture(FTexture&& Other) noexcept;
	FTexture& operator=(FTexture&& Other) noexcept;

	bool Initialize(IRenderDevice& InRenderDevice, const FTextureDesc& InDesc, std::span<const FTextureSubresourceData> InitialData = {});

	const FTextureDesc& GetDesc() const { return Desc; }
	FTextureHandle GetHandle() const { return TextureHandle; }
	bool IsValid() const { return IsInitialized() && TextureHandle.IsValid(); }

protected:
	void OnRelease() override;

private:
	IRenderDevice* RenderDevice = nullptr;
	FTextureDesc Desc;
	FTextureHandle TextureHandle;
};
