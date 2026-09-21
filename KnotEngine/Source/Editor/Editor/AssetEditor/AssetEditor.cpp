#include "Editor/AssetEditor/AssetEditor.h"

#include "Asset/Asset/Asset.h"
#include "Render/RenderSystem.h"
#include "Runtime/EditorEngine.h"
#include "Editor/Toolbar/ViewportToolbar.h"
#include "World/World.h"

#include <algorithm>
#include <imgui.h>

FAssetEditor::FAssetEditor(UEditorEngine& InEditorEngine, UAsset& InAsset)
	: EditorEngine(InEditorEngine), RenderSystem(InEditorEngine.GetRenderSystem()), Asset(InAsset), AssetId(InAsset.GetAssetId()),
	  ViewportWidget(RenderSystem, InEditorEngine.GetInputRouter()), ViewportClient(ViewportWidget.GetViewport())
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
void FAssetEditor::Draw(float DeltaTime, FViewportToolbar& ViewportToolbar)
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
		ViewportWidget.Release(ViewportClient);
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

	const float DetailsWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.28f, 280.0f, 420.0f);
	if (ImGui::BeginChild("##AssetViewport", ImVec2(-DetailsWidth, 0.0f), ImGuiChildFlags_Borders))
	{
		DrawToolbar(ViewportToolbar);
		ViewportWidget.Draw(ViewportClient);
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

	ViewportWidget.Release(ViewportClient);
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
	bStarted = false;
}

UWorld* FAssetEditor::GetPreviewWorld() const
{
	return PreviewWorldContextId != 0 ? EditorEngine.FindWorld(PreviewWorldContextId) : nullptr;
}

void FAssetEditor::DrawToolbar(FViewportToolbar& ViewportToolbar)
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
