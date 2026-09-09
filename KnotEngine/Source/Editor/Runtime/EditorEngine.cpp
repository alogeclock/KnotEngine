#include "EditorEngine.h"

#include "Component/CubeComponent.h"
#include "Component/MovementComponent.h"
#include "World/World.h"
#include "Core/Assert.h"
#include "Render/RHI/RenderTypes.h"

UEditorEngine::UEditorEngine()
	: RenderContext(RenderDevice), Renderer(RenderDevice, RenderContext),
	  ImGuiRenderBackend(RenderDevice), LevelViewport(RenderDevice),
	  EditorUISystem(ImGuiRenderBackend, InputRouter, LevelViewport, LevelViewportClient)
{
}

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	check(EditorContextId == 0);
	checkf(InWindow.GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	Renderer.Create(InWindow.GetHwnd());
	EditorUISystem.Startup(InWindow.GetHwnd());
	EditorContextId = CreateWorldContext(EWorldType::Editor);
	UWorld* EditorWorld = FindWorld(EditorContextId);

	// 테스트용 Cube Node를 생성하고, RendererComponent와 MovementComponent를 추가한다.
	// UI/PIE 분리 전에는 이 데모 World를 직접 실행한다.
	check(EditorWorld);
	UWorld& World = *EditorWorld;
	UNode& Cube = World.GetPersistentLevel().CreateNode(FName("Cube"));
	Cube.AddComponent<UCubeComponent>(Renderer);
	Cube.AddComponent<UMovementComponent>();
	World.BeginPlay();
}

void UEditorEngine::ProcessInput(const FInputSnapshot& InputSnapshot)
{
	InputRouter.BeginFrame(InputSnapshot);
}

void UEditorEngine::Tick(float DeltaTime)
{
	UWorld* World = FindWorld(EditorContextId);
	check(World);

	Renderer.BeginFrame();

	EditorUISystem.BeginFrame();
	EditorUISystem.Draw(*World, DeltaTime); // TO-DO: World를 인자로 넘기는 구조 개선
	InputRouter.RouteInput();

	World->Tick(DeltaTime);
	const FRenderViewport ViewportInfo = LevelViewport.GetRenderViewport();
	const float AspectRatio = ViewportInfo.Height > 0.0f ? ViewportInfo.Width / ViewportInfo.Height : 1.0f;
	const FMatrix View = FMatrix::MakeLookAt(FVector(-5.0f, 0.0f, 0.0f), FVector::ZeroVector, FVector::UpVector);
	const FMatrix Projection = FMatrix::MakePerspectiveFov(KMath::ToRadian(60.0f), AspectRatio, 0.1f, 100.0f);
	if (EditorUISystem.IsViewportVisible() && LevelViewport.IsValid())
	{
		Renderer.BeginRenderTarget(LevelViewport.GetColorTarget(), LevelViewport.GetDepthTarget(), ViewportInfo);
		World->Render(Renderer, View * Projection);
		Renderer.EndRenderTarget();
	}

	EditorUISystem.Render(Renderer.GetCommandList());

	Renderer.EndFrame();
}

void UEditorEngine::Shutdown()
{
	if (UWorld* World = FindWorld(EditorContextId))
	{
		World->EndPlay();
	}
	DestroyWorldContext(EditorContextId);
	EditorContextId = 0;

	InputRouter.Reset();
	EditorUISystem.Shutdown();
	LevelViewport.Release();
	Renderer.Release();
}
