#pragma once

#include "EngineAPI.h"

#include "Object/Object.h"
#include "Platform/WindowsWindow.h"
#include "GameFramework/WorldContext.h"

class FInputSnapshot;
enum class EWorldType : uint8;
class UWorld;

UCLASS()
class ENGINE_API UEngine : public UObject
{
	GENERATED_CLASS(UEngine, UObject)

public:
	~UEngine() override;

	void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Life Cycle & Input
	virtual void Startup(FWindowsWindow InWindow) {}
	virtual void ProcessInput(const FInputSnapshot& InputSnapshot) {}
	virtual void Tick(float DeltaTime) {}
	virtual void Shutdown() {}

	// Context와 해당 World를 함께 생성하고 파괴한다.
	uint64 CreateWorldContext(EWorldType WorldType);
	UWorld* FindWorld(uint64 ContextId) const;
	void DestroyWorldContext(uint64 ContextId);

private:
	TArray<FWorldContext> WorldContexts;
	uint64 NextContextId = 1;
};

extern ENGINE_API UEngine* GEngine;
