#pragma once

#include "World/Node.h"

// World에 속한 Node의 저장 및 관리 단위.
// 공간상의 부모-자식 관계와 관계없이 소속된 모든 Node를 평탄한 배열로 소유하며,
// World의 플레이 생명주기와 Tick 요청을 각 Node에 전달한다.
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

private:
	UPROPERTY(NoEdit, Transient) TObjectPtr<UWorld> OwningWorld;
	UPROPERTY(NoEdit, Transient) TArray<TObjectPtr<UNode>> Nodes;
};
