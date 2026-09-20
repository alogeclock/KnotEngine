#include "EditorEngine.h"
#include "Platform/WindowsApplication.h"

#include "World/MapSerializer.h"
#include "World/World.h"
#include "World/Level.h"
#include "World/Node.h"
#include "Component/Mesh/StaticMeshComponent.h"
#include "Render/RenderSystem.h"
#include "Viewport/EditorViewportClient.h"
#include "Core/Assert.h"
#include "Core/IO/Paths.h"
#include "Core/Log.h"
#include "Core/Profiling/CPUProfiler.h"

#include <algorithm>

UEditorEngine::UEditorEngine(FWindowsApplication& Application, FRenderSystem& InRenderSystem)
    : RenderSystem(InRenderSystem),
      ImGuiSystem(Application, *this, GetAssetManager().GetAssetRegistry(), AssetImportManager, EditorSettings, InRenderSystem, InputRouter, EditorSelection)
{
}

void UEditorEngine::Startup(FWindowsApplication& Application)
{
	check(EditorContextId == 0);
	checkf(Application.GetWindow().GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	if (!EditorSettings.Load())
	{
		KE_LOG(LogEditor, Error, "Editor Settings를 불러오거나 저장하지 못했다. Path={}", FPaths::ToUtf8(FPaths::EditorSettingsPath()));
	}
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
	RenderSystem.ResizeWindow(Size.Width, Size.Height);
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
	GetAssetManager().GetAssetRegistry().Scan();

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

		for (const FImportedAsset& ImportedAsset : Result.ImportedAssets)
		{
			if (ImportedAsset.Type != EAssetType::StaticMesh)
			{
				continue;
			}

			UStaticMesh* ExistingMesh = GetAssetManager().FindStaticMesh(ImportedAsset.AssetId);
			if (!GetAssetManager().ReloadStaticMesh(ImportedAsset.AssetId))
			{
				KE_LOG(LogAssetImporter, Error, "Static Mesh Reload 실패. AssetPath={}, AssetId={}", ImportedAsset.AssetPath, ImportedAsset.AssetId.ToString());
				continue;
			}
			if (!ExistingMesh)
			{
				continue;
			}

			for (UObject* Object : GUObjectArray)
			{
				if (!Object || !Object->IsA(UStaticMeshComponent::StaticClass()))
				{
					continue;
				}

				auto* MeshComponent = static_cast<UStaticMeshComponent*>(Object);
				if (!MeshComponent->IsRegistered() || MeshComponent->GetStaticMesh() != ExistingMesh)
				{
					continue;
				}

				MeshComponent->EnqueueRenderCommand(ERenderCommandType::Mesh | ERenderCommandType::Material);
			}
		}
		KE_LOG(LogAssetImporter, Display, "GLB Import 완료. Source={}, AssetCount={}", SourcePath, Result.ImportedAssets.size());
	}
}

void UEditorEngine::Render()
{
	KNOT_PROFILE_SCOPE("Render", "UEditorEngine::Render");

	TArray<FScene*> Scenes;
	for (FWorldContext& Context : WorldContexts)
	{
		if (UWorld* World = Context.World.Get())
		{
			Scenes.push_back(&World->GetScene());
		}
	}
	TArray<FSceneViewFamily> ViewFamilies;
	for (FEditorViewportClient* ViewportClient : AllViewportClients)
	{
		if (ViewportClient)
		{
			if (auto Family = ViewportClient->BuildSceneViewFamily())
			{
				for (FSceneView& View : Family->Views)
				{
					View.LODSteps = EditorSettings.GetLODSteps();
				}
				ViewFamilies.push_back(std::move(*Family));
			}
		}
	}

	RenderSystem.Render(std::move(Scenes), std::move(ViewFamilies), ImGuiSystem.Consume());
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
	AssetImportManager.Shutdown();
	InputRouter.Reset();
	ImGuiSystem.Shutdown();

	if (UWorld* World = GetWorld())
	{
		World->EndPlay();
		World->Reset();
		RenderSystem.Flush(World->GetScene());
	}

	DestroyWorldContext(EditorContextId);
	EditorContextId = 0;

	RenderSystem.ReleaseAssetResources();
	Super::Shutdown();
}
