#include "Render/Scene/Scene.h"
#include "Core/Assert.h"
#include "Render/Renderer.h"
#include <algorithm>
FScene::~FScene()
{
	// World는 Scene보다 먼저 Component 등록을 해제하여 Proxy를 제거한다.
	check(Proxies.empty() && PendingRenderCommands.empty());
}

uint64 FScene::AddPrimitive(std::unique_ptr<FPrimitiveSceneProxy> Proxy, FPrimitiveRenderData&& RenderData)
{
	check(Proxy);
	check(NextPrimitiveId != 0);
	const uint64 PrimitiveId = NextPrimitiveId++;
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Add, ERenderCommandType::All, PrimitiveId, std::move(RenderData), std::move(Proxy) });
	return PrimitiveId;
}

void FScene::UpdatePrimitive(uint64 PrimitiveId, ERenderCommandType Type, FPrimitiveRenderData&& RenderData)
{
	check(PrimitiveId != 0 && Type != ERenderCommandType::None);
	for (auto Iterator = PendingRenderCommands.rbegin(); Iterator != PendingRenderCommands.rend(); ++Iterator)
	{
		if (Iterator->PrimitiveId != PrimitiveId)
		{
			continue;
		}
		check(Iterator->Action != EPrimitiveCommandAction::Remove);
		Merge(Iterator->RenderData, std::move(RenderData), Type);
		Iterator->Type |= Type;
		return;
	}
	PendingRenderCommands.push_back({ EPrimitiveCommandAction::Update, Type, PrimitiveId, std::move(RenderData) });
}

void FScene::RemovePrimitive(uint64 PrimitiveId)
{
	bool bCanceledPendingAdd = false;
	for (auto Iterator = PendingRenderCommands.begin(); Iterator != PendingRenderCommands.end();)
	{
		if (Iterator->PrimitiveId != PrimitiveId)
		{
			++Iterator;
			continue;
		}
		bCanceledPendingAdd = bCanceledPendingAdd || Iterator->Action == EPrimitiveCommandAction::Add;
		Iterator = PendingRenderCommands.erase(Iterator);
	}
	if (!bCanceledPendingAdd)
	{
		PendingRenderCommands.push_back({ EPrimitiveCommandAction::Remove, ERenderCommandType::All, PrimitiveId });
	}
}

TArray<FPrimitiveRenderCommand> FScene::DrainRenderCommands()
{
	TArray<FPrimitiveRenderCommand> Commands = std::move(PendingRenderCommands);
	PendingRenderCommands.clear();
	return Commands;
}

// Render Thread의 프레임 시작 시 FIFO 명령을 적용한다.
void FScene::ApplyRenderCommands(URenderer& Renderer, TArray<FPrimitiveRenderCommand>&& Commands)
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
		Destination.Mesh = Source.Mesh;
		Destination.MeshAssetId = Source.MeshAssetId;
		Destination.MeshRevision = Source.MeshRevision;
		Destination.bLODEnable = Source.bLODEnable;
	}

	if (HasRenderCommand(Type, ERenderCommandType::Material))
	{
		Destination.Materials = std::move(Source.Materials);
	}

	if (HasRenderCommand(Type, ERenderCommandType::Visibility))
	{
		Destination.bVisible = Source.bVisible;
		Destination.bSelected = Source.bSelected;
	}
}
