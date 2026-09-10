#include "EditorEngine.h"

#include "Component/CubeComponent.h"
#include "Component/MovementComponent.h"
#include "World/World.h"
#include "Render/RHI/RenderTypes.h"
#include "Viewport/EditorViewportClient.h"
#include "Core/Assert.h"

#include <algorithm>

UEditorEngine::UEditorEngine()
	: RenderContext(RenderDevice), Renderer(RenderDevice, RenderContext),
	  ImGuiRenderBackend(RenderDevice), ImGuiSystem(*this, RenderDevice, ImGuiRenderBackend, InputRouter)
{
}

void UEditorEngine::Startup(FWindowsWindow InWindow)
{
	check(EditorContextId == 0);
	checkf(InWindow.GetHwnd(), "창 생성이 끝나기 전에 UEditorEngine::Startup() 호출.");

	Renderer.Create(InWindow.GetHwnd());
	ImGuiSystem.Startup(InWindow.GetHwnd());
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

void UEditorEngine::OnWindowResized(FWindowSize Size)
{
	Renderer.Resize(Size.Width, Size.Height);
}

void UEditorEngine::Tick(float DeltaTime)
{
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

void UEditorEngine::Render()
{
	const FRenderViewport OutputViewport = Renderer.GetViewport();
	if (OutputViewport.Width <= 0.0f || OutputViewport.Height <= 0.0f)
	{
		return;
	}

	Renderer.BeginFrame();

	for (FEditorViewportClient* ViewportClient : AllViewportClients)
	{
		if (ViewportClient)
		{
			ViewportClient->Draw(Renderer);
		}
	}

	ImGuiSystem.Render(Renderer.GetCommandList());

	Renderer.EndFrame();
}

UWorld* UEditorEngine::GetWorld() const
{
	return FindWorld(EditorContextId);
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

void UEditorEngine::Shutdown()
{
	if (UWorld* World = GetWorld())
	{
		World->EndPlay();
	}
	DestroyWorldContext(EditorContextId);
	EditorContextId = 0;

	InputRouter.Reset();
	ImGuiSystem.Shutdown();
	Renderer.Release();
}
