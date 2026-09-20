#include "EditorEngine.h"
#include "Platform/WindowsApplication.h"

#include "World/MapSerializer.h"
#include "World/World.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Scene/SceneRenderer.h"
#include "Viewport/EditorViewportClient.h"
#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/Profiling/CPUProfiler.h"

#include <algorithm>

UEditorEngine::UEditorEngine(FWindowsApplication& Application)
	: RenderBackend(CreateRenderBackend()),
	  Renderer(RenderBackend->GetRenderDevice(), RenderBackend->GetRenderContext(), RenderBackend->GetShaderFormat()),
	  ImGuiSystem(
		  Application,
		  *this,
		  GetAssetManager().GetAssetRegistry(),
		  AssetImportManager,
		  RenderBackend->GetRenderDevice(),
		  RenderBackend->GetImGuiRenderBackend(),
		  InputRouter,
		  EditorSelection)
{
}

void UEditorEngine::Startup(FWindowsApplication& Application)
{
	check(EditorContextId == 0);
	checkf(Application.GetWindow().GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	Renderer.Create(Application.GetWindow().GetHwnd());
	AssetImportManager.Startup();
	ImGuiSystem.Startup();

	EditorContextId = CreateWorldContext(EWorldType::Editor);
	UWorld* EditorWorld = FindWorld(EditorContextId);

	check(EditorWorld);
	EditorWorld->BeginPlay();
}

void UEditorEngine::ProcessInput(const FInputSnapshot& InputSnapshot)
{
	InputRouter.BeginFrame(InputSnapshot);
}

void UEditorEngine::OnWindowResized(FWindowSize Size)
{
	Renderer.Resize(Size.Width, Size.Height);
}

void UEditorEngine::Tick(float DeltaTime)
{
	KNOT_PROFILE_SCOPE("Tick", "UEditorEngine::Tick");

	ProcessAssetImports();
	ImGuiSystem.BeginFrame();
	ImGuiSystem.Draw(DeltaTime);
	InputRouter.RouteInput();

	for (FWorldContext& Context : WorldContexts)
	{
		if (UWorld* World = Context.World.Get())
		{
			World->Tick(DeltaTime);
		}
	}

	for (FEditorViewportClient* ViewportClient : AllViewportClients)
	{
		if (ViewportClient)
		{
			ViewportClient->Tick(DeltaTime);
		}
	}

	ImGuiSystem.EndFrame();

	Render();
}

// Worker Thread의 Import 완료 결과를 Main Thread에서 로그와 Asset Registry에 반영한다.
void UEditorEngine::ProcessAssetImports()
{
	TArray<FAssetImportCompletion> Completions;
	AssetImportManager.DrainCompleted(Completions);
	if (Completions.empty())
	{
		return;
	}

	for (const FAssetImportCompletion& Completion : Completions)
	{
		const FAssetImportResult& Result = Completion.Result;
		const FString SourcePath = FPaths::ToUtf8(Completion.SourceFilePath.wstring());
		if (!Result.bSucceeded)
		{
			KE_LOG(LogAssetImporter, Error, "GLB Import 실패. Source={}, Error={}", SourcePath, Result.Error);
			continue;
		}
		for (const FString& Warning : Result.Warnings)
		{
			KE_LOG(LogAssetImporter, Warning, "GLB Import 경고. Source={}, Warning={}", SourcePath, Warning);
		}
		KE_LOG(LogAssetImporter, Display, "GLB Import 완료. Source={}, AssetCount={}", SourcePath, Result.ImportedAssets.size());
	}
	GetAssetManager().GetAssetRegistry().Scan();
}

void UEditorEngine::Render()
{
	KNOT_PROFILE_SCOPE("Render", "UEditorEngine::Render");

	const FRenderViewport OutputViewport = Renderer.GetViewport();
	if (OutputViewport.Width <= 0.0f || OutputViewport.Height <= 0.0f)
	{
		return;
	}

	TArray<FSceneViewFamily> ViewFamilies;
	for (FEditorViewportClient* ViewportClient : AllViewportClients)
	{
		if (ViewportClient)
		{
			if (auto Family = ViewportClient->BuildSceneViewFamily())
			{
				ViewFamilies.push_back(std::move(*Family));
			}
		}
	}

	Renderer.BeginFrame();
	for (const FSceneViewFamily& Family : ViewFamilies)
	{
		FSceneRenderer SceneRenderer(Family);
		SceneRenderer.Render(Renderer);
	}
	ImGuiSystem.Render(Renderer.GetCommandList());
	Renderer.EndFrame();
}

void UEditorEngine::RegisterViewportClient(FEditorViewportClient& ViewportClient)
{
	if (std::find(AllViewportClients.begin(), AllViewportClients.end(), &ViewportClient) == AllViewportClients.end())
	{
		AllViewportClients.push_back(&ViewportClient);
	}
}

void UEditorEngine::UnregisterViewportClient(FEditorViewportClient& ViewportClient)
{
	AllViewportClients.erase(std::remove(AllViewportClients.begin(), AllViewportClients.end(), &ViewportClient), AllViewportClients.end());
}

// 현재 World를 비우고 아직 저장 경로가 없는 새 Level 상태로 전환한다.
void UEditorEngine::NewLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	EditorSelection.Deselect();
	World->EndPlay();
	World->Reset();
	World->BeginPlay();
	CurrentLevelPath.clear();
}

// 지정한 .kmap을 현재 World에 불러오고 현재 Level 경로를 갱신한다.
bool UEditorEngine::LoadLevel(const std::filesystem::path& FilePath)
{
	UWorld* World = GetWorld();
	if (!World || FilePath.empty())
	{
		return false;
	}

	EditorSelection.Deselect();
	World->EndPlay();
	FMapSerializer Serializer;
	const bool bLoaded = Serializer.Load(*World, FilePath);
	if (bLoaded)
	{
		CurrentLevelPath = FilePath;
	}
	World->BeginPlay();
	return bLoaded;
}

// 지정한 경로 또는 기존 Level 경로에 현재 World를 저장한다.
bool UEditorEngine::SaveLevel(const std::filesystem::path& FilePath)
{
	UWorld* World = GetWorld();
	const std::filesystem::path TargetPath = FilePath.empty() ? CurrentLevelPath : FilePath;
	if (!World || TargetPath.empty())
	{
		return false;
	}

	FMapSerializer Serializer;
	if (!Serializer.Save(*World, TargetPath))
	{
		return false;
	}
	CurrentLevelPath = TargetPath;
	return true;
}

void UEditorEngine::Shutdown()
{
	EditorSelection.Deselect();
	if (UWorld* World = GetWorld())
	{
		World->EndPlay();
	}
	DestroyWorldContext(EditorContextId);
	EditorContextId = 0;

	AssetImportManager.Shutdown();
	InputRouter.Reset();
	ImGuiSystem.Shutdown();
	Super::Shutdown();
	Renderer.Release();
}
