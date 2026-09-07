#pragma once

#include "GameFramework/Node.h"

UCLASS()
class ENGINE_API ULevel : public UObject
{
	GENERATED_CLASS(ULevel, UObject)

public:
	explicit ULevel(UWorld& World);
	~ULevel() override;
	
	UWorld& GetWorld() const;

	UNode& CreateNode(FName Name);
	const TArray<TObjectPtr<UNode>>& GetNodes() const { return Nodes; }

	void BeginPlay();
	void EndPlay();
	void Tick(float DeltaTime);
	void Render(URenderer& Renderer, const FMatrix& ViewProjection) const;

private:
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UNode>> Nodes;
	UPROPERTY(NoEdit, Transient) TObjectPtr<UWorld> OwningWorld;
};
