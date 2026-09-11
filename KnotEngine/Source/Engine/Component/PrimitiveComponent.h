#pragma once

#include "Component/Component.h"
#include <memory>

struct FPrimitiveSceneProxy;

UCLASS()
class ENGINE_API UPrimitiveComponent : public UComponent
{
	GENERATED_CLASS(UPrimitiveComponent, UComponent)

public:
	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible);
	void MarkPrimitiveSceneProxy();
	void PostEditProperty(const FProperty& Property) override;

protected:
	virtual std::unique_ptr<FPrimitiveSceneProxy> CreatePrimitiveSceneProxy() const = 0;
	void OnRegister() override;
	void OnUnregister() override;

private:
	friend struct FPrimitiveSceneProxy;
	FPrimitiveSceneProxy* SceneProxy = nullptr; // FScene 소유, 등록 동안 주소가 유지된다.
	UPROPERTY(Category = "Rendering") bool bVisible = true;
};
