#pragma once

#include "EngineAPI.h"

#include "Asset/AssetManager.h"
#include "Object/Object.h"
#include "Platform/WindowsWindow.h"
#include "World/WorldContext.h"

class FInputSnapshot;
class FWindowsApplication;
enum class EWorldType : uint8;
class UWorld;

UCLASS()
class ENGINE_API UEngine : public UObject
{
	GENERATED_CLASS(UEngine, UObject)

public:
	UEngine();
	~UEngine() override;

	void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual void Startup(FWindowsApplication& Application) {}
	virtual void ProcessInput(const FInputSnapshot& InputSnapshot) {}
	virtual void OnWindowResized(FWindowSize Size) {}
	virtual void Tick(float DeltaTime) {}
	virtual void Shutdown();

	FAssetManager& GetAssetManager() { return AssetManager; }
	const FAssetManager& GetAssetManager() const { return AssetManager; }

	uint64 CreateWorldContext(EWorldType WorldType);
	UWorld* FindWorld(uint64 ContextId) const;
	void DestroyWorldContext(uint64 ContextId);

	virtual UWorld* GetWorld() const;

protected:
	TArray<FWorldContext> WorldContexts;

private:
	FAssetManager AssetManager;
	uint64 NextContextId = 1;
};

extern ENGINE_API UEngine* GEngine;
