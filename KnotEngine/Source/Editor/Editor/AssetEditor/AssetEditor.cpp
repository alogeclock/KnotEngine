#include "Editor/AssetEditor/AssetEditor.h"

#include "Asset/Asset/Asset.h"
#include "Input/InputRouter.h"
#include "Render/RenderSystem.h"
#include "Runtime/EditorEngine.h"
#include "Editor/Toolbar/ViewportToolbar.h"
#include "World/World.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

FAssetEditor::FAssetEditor(
	UEditorEngine& InEditorEngine,
	FRenderSystem& InRenderSystem,
	FInputRouter& InInputRouter,
	FViewportToolbar& InViewportToolbar,
	UAsset& InAsset)
	: EditorEngine(InEditorEngine), RenderSystem(InRenderSystem), InputRouter(InInputRouter), ViewportToolbar(InViewportToolbar), Asset(InAsset),
	  AssetId(InAsset.GetAssetId()), Viewport(InRenderSystem), ViewportClient(Viewport)
{
}

FAssetEditor::~FAssetEditor()
{
	Release();
}

// Preview World를 만들고 Viewport Client를 Editor의 공통 Tick 및 Render 수집 경로에 등록한다.
void FAssetEditor::Startup()
{
	check(!bStarted && PreviewWorldContextId == 0);
	PreviewWorldContextId = EditorEngine.CreateWorldContext(EWorldType::Asset);
	UWorld* PreviewWorld = GetPreviewWorld();
	panicf(PreviewWorld, "Asset Editor Preview World 생성 실패. AssetId={}", AssetId.ToString());

	ViewportClient.SetWorld(PreviewWorld);
	CreatePreviewContent(*PreviewWorld);
	EditorEngine.RegisterViewportClient(ViewportClient);

	const FString& AssetPath = Asset.GetAssetPath();
	const SIZE_T Separator = AssetPath.find_last_of('/');
	const FString AssetName = Separator == FString::npos ? AssetPath : AssetPath.substr(Separator + 1);
	WindowName = AssetName + " - " + GetEditorTypeName() + "###AssetEditor_" + AssetId.ToString();
	bStarted = true;
}

// 공통 Toolbar, Preview Viewport와 Asset별 Details를 하나의 동적 Document Panel에 배치한다.
void FAssetEditor::Draw(float DeltaTime)
{
	(void)DeltaTime;
	check(bStarted);
	if (bFocusRequested)
	{
		ImGui::SetNextWindowFocus();
		bFocusRequested = false;
	}
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 700.0f), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (!ImGui::Begin(WindowName.c_str(), &bOpen))
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	const float DetailsWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.28f, 280.0f, 420.0f);
	if (ImGui::BeginChild("##AssetViewport", ImVec2(-DetailsWidth, 0.0f), ImGuiChildFlags_Borders))
	{
		DrawToolbar();
		DrawViewport();
	}
	ImGui::EndChild();
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
	if (ImGui::BeginChild("##AssetDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
	{
		DrawAssetDetails();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();

	ImGui::End();
	ImGui::PopStyleVar();
}

// 렌더 스레드가 Preview Scene을 더 이상 참조하지 않는 시점까지 기다린 뒤 Preview World와 Target을 해제한다.
void FAssetEditor::Release()
{
	if (!bStarted)
	{
		return;
	}

	InputRouter.UnregisterTarget(ViewportClient);
	EditorEngine.UnregisterViewportClient(ViewportClient);
	DestroyPreviewContent();
	if (UWorld* PreviewWorld = GetPreviewWorld())
	{
		PreviewWorld->Reset();
		RenderSystem.Flush(PreviewWorld->GetScene());
	}
	ViewportClient.SetWorld(nullptr);
	EditorEngine.DestroyWorldContext(PreviewWorldContextId);
	PreviewWorldContextId = 0;
	Viewport.Release();
	bStarted = false;
}

UWorld* FAssetEditor::GetPreviewWorld() const
{
	return PreviewWorldContextId != 0 ? EditorEngine.FindWorld(PreviewWorldContextId) : nullptr;
}

void FAssetEditor::DrawToolbar()
{
	ViewportToolbar.Draw(ViewportClient, [this]
	{
		ImGui::TextUnformatted(GetEditorTypeName());
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		DrawAssetToolbar();
	});
}

void FAssetEditor::DrawViewport()
{
	const ImVec2 ImageSize = ImGui::GetContentRegionAvail();
	if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		return;
	}

	const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
	const uint32 Width = static_cast<uint32>(std::lround(ImageSize.x * FramebufferScale.x));
	const uint32 Height = static_cast<uint32>(std::lround(ImageSize.y * FramebufferScale.y));
	if (Width == 0 || Height == 0)
	{
		InputRouter.UnregisterTarget(ViewportClient);
		Viewport.Release();
		return;
	}

	Viewport.Resize(Width, Height);
	if (!Viewport.IsValid())
	{
		return;
	}

	ImGui::Image(ImTextureRef(Viewport.GetDisplayTextureId()), ImageSize);
	const ImVec2 ImagePosition = ImGui::GetItemRectMin();
	ViewportClient.SetInputRect(FVector2(ImagePosition.x, ImagePosition.y), FVector2(ImageSize.x, ImageSize.y));
	InputRouter.RegisterTarget(ViewportClient, ImGui::IsItemHovered(), ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
}
