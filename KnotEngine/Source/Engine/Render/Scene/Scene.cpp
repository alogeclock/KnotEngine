#include "Render/Scene/Scene.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include <algorithm>
FScene::~FScene()
{
	// World는 Scene보다 먼저 Component 등록을 해제하여 Proxy를 제거한다.
	check(Proxies.empty() && PendingRenderCommands.empty() && PendingCommandIndices.empty());
}

// 새 Primitive의 Add 명령을 등록하고 이후 갱신이 같은 명령에 병합되도록 인덱스를 저장한다.
uint64 FScene::AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy, FPrimitiveRenderData&& RenderData)
{
	check(Proxy);
	check(NextPrimitiveId != 0);
	const uint64 PrimitiveId = NextPrimitiveId++;
	check(!PendingCommandIndices.contains(PrimitiveId));
	PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Add, ERenderCommandType::All, PrimitiveId, std::move(RenderData), std::move(Proxy) });
	return PrimitiveId;
}

// Primitive별 Pending Command 인덱스로 기존 명령을 상수 시간에 찾고 최신 Render Data를 병합한다.
void FScene::UpdatePrimitive(uint64 PrimitiveId, ERenderCommandType Type, FPrimitiveRenderData&& RenderData)
{
	check(PrimitiveId != 0 && Type != ERenderCommandType::None);
	const auto Iterator = PendingCommandIndices.find(PrimitiveId);
	if (Iterator != PendingCommandIndices.end())
	{
		FPrimitiveRenderCommand& Command = PendingRenderCommands[Iterator->second];
		check(Command.PrimitiveId == PrimitiveId && Command.Action != EPrimitiveCommandAction::Remove);
		Merge(Command.RenderData, std::move(RenderData), Type);
		Command.Type |= Type;
		return;
	}
	PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Update, Type, PrimitiveId, std::move(RenderData) });
}

// Add 전 제거는 명령을 취소하고, 이미 존재하는 Primitive는 Pending 명령을 Remove 하나로 교체한다.
void FScene::RemovePrimitive(uint64 PrimitiveId)
{
	check(PrimitiveId != 0);
	const auto Iterator = PendingCommandIndices.find(PrimitiveId);
	if (Iterator == PendingCommandIndices.end())
	{
		PendingCommandIndices.emplace(PrimitiveId, PendingRenderCommands.size());
		PendingRenderCommands.push_back({ EPrimitiveCommandAction::Remove, ERenderCommandType::All, PrimitiveId });
		return;
	}

	const SIZE_T CommandIndex = Iterator->second;
	FPrimitiveRenderCommand& Command = PendingRenderCommands[CommandIndex];
	check(Command.PrimitiveId == PrimitiveId);
	if (Command.Action == EPrimitiveCommandAction::Add)
	{
		const SIZE_T LastIndex = PendingRenderCommands.size() - 1;
		PendingCommandIndices.erase(Iterator);
		if (CommandIndex != LastIndex)
		{
			PendingRenderCommands[CommandIndex] = std::move(PendingRenderCommands[LastIndex]);
			PendingCommandIndices[PendingRenderCommands[CommandIndex].PrimitiveId] = CommandIndex;
		}
		PendingRenderCommands.pop_back();
		return;
	}
	if (Command.Action == EPrimitiveCommandAction::Remove)
	{
		return;
	}
	Command = { EPrimitiveCommandAction::Remove, ERenderCommandType::All, PrimitiveId };
}

// 제출할 명령 배열을 분리하고 다음 프레임의 Primitive별 Pending Command 인덱스를 초기화한다.
TArray<FPrimitiveRenderCommand> FScene::DrainRenderCommands()
{
	TArray<FPrimitiveRenderCommand> Commands = std::move(PendingRenderCommands);
	PendingRenderCommands.clear();
	PendingCommandIndices.clear();
	return Commands;
}

// Render Thread의 프레임 시작 시 FIFO 명령을 적용한다.
void FScene::ApplyRenderCommands(FRenderer& Renderer, TArray<FPrimitiveRenderCommand>&& Commands)
{
	for (FPrimitiveRenderCommand& Command : Commands)
	{
		if (Command.Action == EPrimitiveCommandAction::Add)
		{
			check(Command.Proxy && !ProxyIndices.contains(Command.PrimitiveId));
			Command.Proxy->Apply(ERenderCommandType::All, Command.RenderData, Renderer);
			Command.Proxy->SceneIndex = Proxies.size();
			ProxyIndices.emplace(Command.PrimitiveId, Command.Proxy->SceneIndex);
			Proxies.push_back(std::move(Command.Proxy));
			continue;
		}

		const auto Iterator = ProxyIndices.find(Command.PrimitiveId);
		check(Iterator != ProxyIndices.end());
		const SIZE_T Index = Iterator->second;
		if (Command.Action == EPrimitiveCommandAction::Update)
		{
			Proxies[Index]->Apply(Command.Type, Command.RenderData, Renderer);
			continue;
		}

		const SIZE_T LastIndex = Proxies.size() - 1;
		if (Index != LastIndex)
		{
			std::swap(Proxies[Index], Proxies[LastIndex]);
			Proxies[Index]->SceneIndex = Index;
			for (auto& [PrimitiveId, ProxyIndex] : ProxyIndices)
			{
				if (ProxyIndex == LastIndex)
				{
					ProxyIndex = Index;
					break;
				}
			}
		}
		Proxies.pop_back();
		ProxyIndices.erase(Command.PrimitiveId);
	}
}

void FScene::Merge(FPrimitiveRenderData& Destination, FPrimitiveRenderData&& Source, ERenderCommandType Type)
{
	if (HasRenderCommand(Type, ERenderCommandType::Transform))
	{
		Destination.WorldMatrix = Source.WorldMatrix;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Mesh))
	{
		Destination.WorldMatrix = Source.WorldMatrix;
		Destination.LocalBounds = Source.LocalBounds;
		Destination.MeshAssetId = Source.MeshAssetId;
		Destination.bLODEnable = Source.bLODEnable;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Material))
	{
		Destination.MaterialAssetIds = std::move(Source.MaterialAssetIds);
	}

	if (HasRenderCommand(Type, ERenderCommandType::Visibility))
	{
		Destination.bVisible = Source.bVisible;
		Destination.bSelected = Source.bSelected;
	}
}
