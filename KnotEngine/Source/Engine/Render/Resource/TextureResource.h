#pragma once

#include "EngineAPI.h"

#include "Render/Resource/RenderResource.h"
#include "Render/RHI/RenderTypes.h"

#include <span>

class IRenderDevice;

// Texture Asset의 Mip 데이터로 생성한 GPU Texture를 Renderer의 Resource Cache가 소유한다.
class ENGINE_API FTextureResource final : public FRenderResource
{
public:
	FTextureResource() = default;
	~FTextureResource() override;
	FTextureResource(FTextureResource&& Other) noexcept;
	FTextureResource& operator=(FTextureResource&& Other) noexcept;

	bool Initialize(IRenderDevice& InRenderDevice, const FTextureDesc& InDesc, std::span<const FTextureSubresourceData> InitialData, uint64 InSourceRevision);

	const FTextureDesc& GetDesc() const { return Desc; }
	FTextureHandle GetHandle() const { return TextureHandle; }
	uint64 GetSourceRevision() const { return SourceRevision; }
	bool IsValid() const { return IsInitialized() && TextureHandle.IsValid(); }

protected:
	void OnRelease() override;

private:
	IRenderDevice* RenderDevice = nullptr;
	FTextureDesc Desc;
	FTextureHandle TextureHandle;
	uint64 SourceRevision = 0;
};
