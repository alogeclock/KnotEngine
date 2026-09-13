#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"

#include <functional>

// 한 ViewFamily의 임시 Pass Node와 실행 의존성을 보관하고 프레임 안에서 소비한다.
class ENGINE_API FRenderGraph
{
public:
	using FExecuteFunction = std::function<void()>;
	static constexpr uint32 InvalidIndex = static_cast<uint32>(-1);

	uint32 AddPass(FString Name, FExecuteFunction ExecuteFunction);
	void AddDependency(uint32 NodeIndex, uint32 DependencyIndex);
	void Execute();

private:
	struct FNode
	{
		FString Name;
		TArray<uint32> Dependencies;
		FExecuteFunction ExecuteFunction;
	};

	TArray<FNode> Nodes;
	bool bExecuted = false;
};
