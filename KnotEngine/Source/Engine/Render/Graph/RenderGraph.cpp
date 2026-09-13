#include "Render/Graph/RenderGraph.h"

#include "Core/Assert.h"

uint32 FRenderGraph::AddPass(FString Name, FExecuteFunction ExecuteFunction)
{
	checkf(!bExecuted, "이미 실행된 Render Graph에는 Pass를 추가할 수 없다.");
	checkf(!Name.empty() && ExecuteFunction, "Render Graph Pass 정보가 비어 있다.");
	checkf(Nodes.size() < InvalidIndex, "Render Graph Node 수가 uint32 범위를 초과했다.");

	FNode& Node = Nodes.emplace_back();
	Node.Name = std::move(Name);
	Node.ExecuteFunction = std::move(ExecuteFunction);
	return static_cast<uint32>(Nodes.size() - 1);
}

void FRenderGraph::AddDependency(uint32 NodeIndex, uint32 DependencyIndex)
{
	checkf(!bExecuted, "이미 실행된 Render Graph에는 의존성을 추가할 수 없다.");
	checkf(NodeIndex < Nodes.size() && DependencyIndex < Nodes.size(), "Render Graph 의존성에 유효하지 않은 Node Index가 전달되었다.");
	checkf(NodeIndex != DependencyIndex, "Render Graph Pass는 자기 자신에 의존할 수 없다.");
	Nodes[NodeIndex].Dependencies.push_back(DependencyIndex);
}

void FRenderGraph::Execute()
{
	checkf(!bExecuted, "Render Graph는 한 번만 실행할 수 있다.");
	bExecuted = true;

	// 나중 노드에 의존한다면 시간 복잡도가 높아질 수 있으나, 렌더 패스의 수가 수천 개로 늘지 않는 한 허용 가능한 수준이다.
	TArray<bool> ExecutedNodes(Nodes.size(), false);
	size_t ExecutedCount = 0;
	while (ExecutedCount < Nodes.size())
	{
		bool bMadeProgress = false;
		for (uint32 NodeIndex = 0; NodeIndex < Nodes.size(); ++NodeIndex)
		{
			if (ExecutedNodes[NodeIndex])
			{
				continue;
			}

			// 의존성이 있는 이전 패스들을 모두 실행했는지 검토한다.
			const FNode& Node = Nodes[NodeIndex];
			bool bExecuted = true;
			for (const uint32 Dependency : Node.Dependencies)
			{
				if (!ExecutedNodes[Dependency])
				{
					bExecuted = false;
					break;
				}
			}
			if (!bExecuted)
			{
				continue;
			}

			Node.ExecuteFunction();
			ExecutedNodes[NodeIndex] = true;
			++ExecutedCount;
			bMadeProgress = true;
		}
		panicf(bMadeProgress, "Render Graph에서 순환 의존성을 감지했다.");
	}
}
