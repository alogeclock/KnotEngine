#include "Render/ImGui/ImGuiDrawDataCopy.h"

#include "Core/Assert.h"

void FImGuiDrawDataCopy::Copy(const ImDrawData* DrawData)
{
	Reset();
	if (!DrawData)
	{
		return;
	}

	DisplayPosition = DrawData->DisplayPos;
	DisplaySize = DrawData->DisplaySize;
	FramebufferScale = DrawData->FramebufferScale;
	DrawLists.reserve(static_cast<SIZE_T>(DrawData->CmdListsCount));
	for (const ImDrawList* SourceList : DrawData->CmdLists)
	{
		check(SourceList);
		FImGuiDrawListCopy& DestinationList = DrawLists.emplace_back();
		DestinationList.Vertices.assign(SourceList->VtxBuffer.begin(), SourceList->VtxBuffer.end());
		DestinationList.Indices.assign(SourceList->IdxBuffer.begin(), SourceList->IdxBuffer.end());
		DestinationList.Commands.reserve(static_cast<SIZE_T>(SourceList->CmdBuffer.Size));
		for (const ImDrawCmd& SourceCommand : SourceList->CmdBuffer)
		{
			checkf(!SourceCommand.UserCallback || SourceCommand.UserCallback == ImDrawCallback_ResetRenderState,
			       "Render Thread ImGui Backend은 임의의 Draw Callback을 지원하지 않는다.");
			DestinationList.Commands.push_back({
				SourceCommand.ClipRect,
				SourceCommand.GetTexID(),
				SourceCommand.ElemCount,
				SourceCommand.IdxOffset,
				SourceCommand.VtxOffset,
				SourceCommand.UserCallback == ImDrawCallback_ResetRenderState,
			});
		}
	}
}

void FImGuiDrawDataCopy::Reset()
{
	DisplayPosition = {};
	DisplaySize = {};
	FramebufferScale = ImVec2(1.0f, 1.0f);
	DrawLists.clear();
}

bool FImGuiDrawDataCopy::IsVisible() const
{
	return DisplaySize.x > 0.0f && DisplaySize.y > 0.0f && !DrawLists.empty();
}
