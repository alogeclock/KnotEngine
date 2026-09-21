#pragma once

#include "Component/Component.h"
#include "Render/Scene/RenderCommand.h"
#include <memory>

struct FPrimitiveSceneProxy;
struct FPrimitiveRenderData;

UCLASS()
class ENGINE_API UPrimitiveComponent : public UComponent
{
	GENERATED_CLASS(UPrimitiveComponent, UComponent)

public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible);
	void PushSelection(bool bSelected);
	void PostEditProperty(const FProperty& Property) override;

	void EnqueueRenderCommand(ERenderCommandType Type = ERenderCommandType::All);
	virtual FPrimitiveRenderData BuildPrimitiveRenderData(ERenderCommandType Type) const = 0;

protected:
	virtual std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy(const FPrimitiveRenderData& RenderData) const = 0;
	void OnRegister() override;
	void OnUnregister() override;

private:
	friend class FScene;

	static constexpr uint64 InvalidPrimitiveId = 0;
	uint64 PrimitiveId = InvalidPrimitiveId;

	/// Primitive을 Scene에 표시할지 결정한다.
	UPROPERTY(Category = "Primitive") bool bVisible = true;
};
