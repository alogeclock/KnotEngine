# Knot Engine Rendering Architecture

## 문서 목적

이 문서는 Knot Engine의 World 렌더 상태, ViewFamily 구성, 가시성 판정, Draw Command 생성과 GPU 실행의 책임을 정의한다. 현재 D3D11 렌더링은 전용 `FRenderThread`에서 실행하며 Game/Editor Thread는 Render Command를 제출한다.

현재 Opaque, Gamma Correction Post Process와 Overlay는 매 프레임 생성되는 Render Graph Node로 구성한다. Shader와 Pipeline State는 공용 Cache가, Static Mesh·Texture·Material의 GPU 대응 객체는 각각 `FStaticMeshResource`, `FTextureResource`, `FMaterialResource`가 소유한다. Asset은 CPU 데이터와 Revision만 소유한다.

## 설계 원칙

- World는 게임 상태와 Scene의 수명을 관리하고 Component 변경은 값을 소유한 Render Command로 전달한다.
- FScene은 PrimitiveId로 식별되는 Proxy를 Render Thread에서 소유하고 Component를 참조하지 않는다.
- Component는 Proxy 주소 대신 PrimitiveId만 보관하며 Transform, Mesh, Material, Visibility 변경 명령을 제출한다.
- Game Thread 제출 큐는 같은 PrimitiveId의 부분 갱신을 병합하고 Render Thread는 Proxy 상태와 GPU Resource만 변경한다.
- ViewportClient는 카메라와 출력 영역으로 View 및 ViewFamily를 구성한다.
- EditorEngine은 World와 ViewFamily를 수집해 `FRenderSystem`에 프레임 렌더를 요청한다.
- SceneRenderer는 가시성·명령 선택·패스 순서를 결정하고 Renderer는 렌더 명령을 실행한다.
- 가시 Primitive 수집과 패스별 Draw Command 생성은 별도 단계다.
- Material UObject의 렌더 바인딩은 Renderer가 소유하는 `FMaterialResource`로 변환해 Draw Command에서 재사용한다.
- 패스마다 목적에 맞는 Sort Key를 사용한다. Scene 전체를 하나의 정렬 순서로 고정하지 않는다.
- RHI 계약은 Engine.dll에, D3D11 구현은 Renderer.dll에 둔다.
- ImGui 화면 구성은 Editor, ImGui GPU 백엔드는 Renderer 모듈의 책임이다.
- 최초 구현에서는 실제 DLL 경계 외의 범용 인터페이스와 Factory를 추가하지 않는다.

## 전체 구조

```text
UEditorEngine
    ├─ WorldContext별 UWorld::Tick
	│      └─ Playing: Level → Node → Component::TickComponent
    ├─ ViewportClient Tick / BuildSceneViewFamily
	└─ FRenderSystem::Render
	       ├─ Scene의 병합된 Render Command Drain
	       └─ FRenderThread FIFO에 Frame 명령 제출
	              ├─ FScene::ApplyRenderCommands
	              ├─ URenderer::BeginFrame
	              ├─ Family별 FSceneRenderer 생성·실행
	              ├─ ImGui 합성
	              └─ URenderer::EndFrame → Submit → Present
```

```text
UPrimitiveComponent [GT] ──PrimitiveId + Render Command──> FScene [RT]
                                                              └─ FPrimitiveSceneProxy[]

FSceneViewFamily ──참조──> FScene
                ├─ FSceneView[]
                ├─ FSceneRenderTarget
                └─ FShowFlags

URenderer ──소유──> FMaterialResource[]
	├─ FStaticMeshResource[] → FStaticMeshLODResource[] → FMeshBuffer
	├─ FTextureResource[]
	└─ Shader / Pipeline / Sampler Cache
```

## 디렉터리와 책임

```text
KnotEngine/Content/Engine/Shader/
├─ StaticMesh.hlsl
├─ PostProcess.hlsl
├─ GeometryMesh.hlsl
├─ Grid.hlsl
└─ Axis.hlsl

KnotEngine/Source/
├─ Engine/
│  ├─ Component/
│  │  ├─ PrimitiveComponent.h/.cpp
│  │  └─ Mesh/StaticMeshComponent.h/.cpp
│  ├─ World/World.h/.cpp
│  └─ Render/
│     ├─ Renderer.h/.cpp
│     ├─ Graph/RenderGraph.h/.cpp
│     ├─ Pass/
│     │  ├─ OpaquePass.h/.cpp
│     │  ├─ PostProcessPass.h/.cpp
│     │  └─ OverlayPass.h/.cpp
│     ├─ Scene/
│     │  ├─ Scene.h/.cpp
│     │  ├─ SceneView.h
│     │  └─ SceneRenderer.h/.cpp
│     ├─ Proxy/
│     │  └─ PrimitiveSceneProxy.h/.cpp
│     ├─ Resource/
│     │  ├─ Buffer.h/.cpp
│     │  ├─ MaterialResource.h/.cpp
│     │  ├─ TextureResource.h/.cpp
│     │  ├─ Mesh/GeometryMesh.h/.cpp
│     │  ├─ Mesh/MeshBuffer.h/.cpp
│     │  ├─ Mesh/StaticMeshResource.h/.cpp
│     │  └─ State/
│     ├─ Shader/
│     └─ RHI/
│        ├─ RenderDevice.h
│        ├─ RenderContext.h
│        ├─ RenderTypes.h
│        └─ VertexLayout.h
├─ Renderer/Render/
│  ├─ RenderSystem.h/.cpp
│  ├─ RenderThread.h/.cpp
│  ├─ D3D11/
│  └─ ImGui/
└─ Editor/
   ├─ Runtime/EditorEngine.h/.cpp
   ├─ Viewport/
   │  ├─ Viewport.h/.cpp
   │  ├─ EditorViewportCamera.h/.cpp
   │  └─ EditorViewportClient.h/.cpp
   └─ Editor/ImGuiSystem.h/.cpp
```

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `UWorld` | Level·Component 수명, Tick, Scene 소유 | Render Proxy 직접 변경 |
| `UPrimitiveComponent` | PrimitiveId 보관, 등록·해제·부분 갱신 명령 제출 | Proxy 주소, GPU Resource 소유 |
| `FPrimitiveSceneProxy` | Render Command로 받은 WorldMatrix·Bounds·Mesh 상태 보관 | Component/UObject 역참조 |
| `FScene` | GT 명령 병합과 RT Proxy 소유·적용 | GPU Resource 소유 |
| `FEditorViewportClient` | 카메라 입력, View·ViewFamily 구성 | SceneRenderer 생성, Draw 실행 |
| `FSceneRenderer` | View별 가시성, 임시 Pass Node 구성과 실행 의존성 선언 | World Tick, Proxy 갱신, Present |
| `URenderer` | Shader·Pipeline Cache와 Asset ID 기반 Mesh/Texture/Material Resource Cache, 프레임·Graph 실행 | Asset UObject 수명, Component 접근 |
| `FMaterialResource` | Material별 Pipeline·상수·Texture Resource/Sampler binding 캐시 | Material UObject 소유, Primitive 공간 상태 |
| `IRenderDevice` | GPU 자원과 Command List 연산 | World·Editor 정책 |
| `IRenderContext` | 네이티브 창 출력, Swap Chain, Resize·Present | Scene 순회 |
| `FRenderSystem` | Renderer·Backend·Render Thread 소유, 명령 직렬화 | World·Asset 로드 정책 |
| `UEditorEngine` | World·ViewFamily·UI 렌더 요청 구성 | GPU API 직접 호출 |

`URenderer`는 이름에 U 접두사가 있지만 현재 UObject를 상속하지 않는 일반 C++ 객체다. `FSceneInterface`, `IRendererModule` 등의 추가 추상 계층은 없다.

## 프레임 실행 순서

### 현재 구현

1. Game/Editor Thread에서 UI, World Tick과 ViewFamily 구성을 마친다.
2. Component 변경 시점에 제출한 Transform, Mesh, Material, Visibility 명령을 PrimitiveId별로 병합한다.
3. `FRenderSystem::Render()`가 Scene 명령과 ViewFamily를 Render Thread FIFO에 제출한다.
4. Render Thread가 Scene Proxy 추가·갱신·제거 명령을 적용한다.
5. Renderer 프레임을 시작하고 Family마다 지역 SceneRenderer가 Render Graph를 구성·실행한다.
6. ImGui draw data를 Back Buffer에 합성한 뒤 Command List를 제출하고 Present한다.

```cpp
Renderer.BeginFrame();
for (const FSceneViewFamily& Family : ViewFamilies)
{
    FSceneRenderer SceneRenderer(Family);
    SceneRenderer.Render(Renderer);
}
ImGuiSystem.Render(Renderer.GetCommandList());
Renderer.EndFrame();
```

World와 Scene 갱신은 Viewport 수에 따라 반복하지 않는다. 현재 Scene은 모든 Proxy를 순회하지만 실제 데이터 복사는 Dirty Proxy에서만 수행한다. 별도 Dirty queue는 없다.

Native 출력 크기가 0이면 GPU 프레임을 생략한다. World Tick과 Scene 갱신은 그보다 앞서 수행된다. 유효한 Family가 없어도 Native 출력이 유효하면 ImGui 합성과 Present를 수행한다.

### 목표 실행 순서

Render Thread 도입 후의 목표 경계는 다음과 같다. 아래 전달 데이터와 스레드 실행 경로는 아직 구현하지 않았다.

```text
Game Thread
  UI·입력 → World 갱신 → 최종 Transform 확정
    → Dirty Component 상태를 전달 데이터로 복사
    → ViewFamily와 프레임 렌더 요청 제출

Render Thread
  Scene 등록·갱신·제거 요청 적용
    → 해당 프레임의 SceneRenderer 작업 생성·실행
    → Render Graph Node별 명령 구성·정렬·기록
    → 준비된 UI 출력 합성 → Submit / Present
```

현재 `FRenderSystem::Render()`는 Render Thread 명령의 완료를 기다려 한 프레임을 직렬 실행한다. GT/RT 프레임 중첩은 ImGui 데이터의 깊은 복사와 프레임 수명 제한을 구현한 뒤 활성화한다.

## Scene 렌더 데이터

### FScene

`FScene`은 Game Thread 제출 큐와 Render Thread Proxy 저장소를 함께 관리한다. Component는 `PrimitiveId`로 Add, Update, Remove 명령을 제출하고 같은 Primitive의 부분 갱신은 프레임 렌더 전에 병합된다. Render Thread는 FIFO 순서로 명령을 적용해 Proxy를 생성·갱신·제거한다.

Proxy는 Component 포인터를 보관하지 않고 Render Command가 소유한 값만 `Apply()`로 받는다. SceneRenderer는 Render Thread에서 `GetProxies()`로 갱신이 완료된 렌더 상태만 조회한다.

### Component와 Proxy

```text
OnRegister
  → PrimitiveId 할당
  → Add + All Render Command 제출

Transform / Mesh / Visibility / Inspector 변경
  → 현재 값을 FPrimitiveRenderData로 추출
  → PrimitiveId + 부분 Render Command 제출
  → 같은 PrimitiveId의 명령 병합

OnUnregister
  → Remove Render Command 제출
  → PrimitiveId 초기화
```

`CreatePrimitiveSceneProxy()`는 PrimitiveComponent의 순수 가상 함수이고 현재 StaticMeshComponent에서 구현한다. Proxy 생성 후에는 `Transform`, `Mesh`, `Material`, `Visibility`, `All` 명령으로 필요한 부분만 교체한다.

`FPrimitiveSceneProxy`는 공통 Primitive 상태를, `FStaticMeshSceneProxy`는 Renderer가 소유한 `FStaticMeshResource`와 `FMaterialResource`의 비소유 참조를 보관한다. CPU Static Mesh와 Material UObject는 명령 적용 뒤 Proxy에 남기지 않는다.

| Proxy 데이터 | 의미 |
|---|---|
| `MeshResource` | LOD별 GPU Buffer와 Section을 가진 Static Mesh Resource 참조 |
| `WorldMatrix` | 로컬 공간에서 월드 공간으로 변환 |
| `LocalBounds` | Mesh의 로컬 AABB |
| `WorldBounds` | WorldMatrix를 적용한 AABB |
| `bVisible` | View와 무관한 표시 상태 |
| `MeshResource` | Renderer Cache가 소유하는 GPU LOD Resource |
| `bLODEnable` | 화면 비율 기반 LOD 선택 사용 여부 |

Mesh가 없거나 CPU Bounds가 유효하지 않으면 Proxy의 Mesh Resource를 비운다. Static Mesh 재임포트는 UObject와 AssetId를 유지하고 CPU Mesh Revision을 증가시킨다. 해당 Mesh를 사용하는 등록 Component에 Mesh/Material 명령을 제출하면 Renderer Cache가 Revision 변경을 감지해 GPU Buffer를 재생성한다.

부모 Transform 변경은 자손 Primitive에 Transform 명령을 제출한다. Stopped·Paused 상태의 Inspector 편집도 변경 시점에 명령을 생성하므로 다음 렌더 요청에 반영된다.

### Material Resource

`FMaterialResource`는 Primitive의 공간 상태를 나타내는 `FPrimitiveSceneProxy`와 별개의 Renderer 소유 Asset Resource다. `UMaterialInterface` 하나를 Pipeline State, Material Constant bytes, Constant Buffer binding과 Texture Resource/Sampler binding으로 변환해 보관한다. Resource는 원본 Material UObject를 보관하지 않는다.

```text
UMaterialInterface
  → URenderer::GetOrCreateMaterialResource
      ├─ 같은 AssetId와 Revision: 기존 FMaterialResource 반환
      └─ 최초 등록 또는 Revision 변경: FMaterialResource 생성·갱신
          ├─ ShaderRegistry에서 Shader 조회·생성
          ├─ PipelineStateCache에서 Pipeline 조회·생성
          ├─ Reflection Layout에 맞춰 Material Constant 패킹
          ├─ FTextureResource 준비
          └─ Texture Resource/Sampler Binding 저장
```

기본 Material과 White Texture는 Cache의 `nullptr` Key가 아니라 `URenderer`가 명시적인 `DefaultMaterialResource`, `DefaultTextureResource`로 소유한다.

`URenderer`는 `FAssetId`를 키로 하는 Map에서 Material Resource를 `unique_ptr`로 소유한다. 따라서 Map 재해시 이후에도 Resource 주소는 유지되고, Draw Command는 Renderer가 해제되기 전까지 안정적인 비소유 포인터를 사용할 수 있다. Renderer는 Shader·Pipeline·Sampler Cache와 Render Device를 해제하기 전에 Material Resource를 먼저 제거한다. Material Resource는 UObject를 보관하지 않고 Renderer가 소유하는 `FTextureResource`를 비소유 참조한다.

각 Material Resource에는 Opaque Sort Key에 사용하는 Renderer 수명 범위의 `SortId`와 원본의 `SourceRevision`이 있다. Renderer는 Asset Revision이 달라지면 같은 Resource 인스턴스를 제자리에서 갱신한다. 현재 Material과 Texture Asset은 최초 로드 시 Revision 1을 부여하지만 실행 중 편집·재임포트 API는 아직 없으므로, 실제 Revision 증가 경로는 해당 기능을 도입할 때 추가한다.

### View와 ViewFamily

`FSceneView`는 ViewMatrix, ProjectionMatrix, ViewProjectionMatrix, ViewOrigin, Frustum과 FRenderViewport를 보관한다. 행벡터 규약으로 `ViewProjectionMatrix = ViewMatrix * ProjectionMatrix`이며 Draw 시 `WorldMatrix * ViewProjectionMatrix`를 사용한다.

`FSceneViewFamily`는 Scene 참조, View 배열, RenderTarget과 ShowFlags를 묶는다. `FSceneRenderTarget`은 선형 Scene Color, 화면 표시용 Color, Depth texture handle과 Width·Height를 보관한다. Family는 Scene이나 GPU 타깃을 소유하지 않는다.

`FEditorViewportClient::GetWorld()`는 기본적으로 `GEngine->GetWorld()`를 사용하고 파생 Client가 다른 World를 선택할 수 있다. `BuildSceneViewFamily()`는 출력과 Scene이 유효할 때 `BuildSceneView()` 결과를 묶는다. 현재 Editor는 Family당 View 하나를 만들고 SceneRenderer는 여러 View를 처리할 수 있다.

`FEditorViewportCameraTransform`, `EEditorViewportViewMode`, `FEditorViewportCamera`는 `EditorViewportCamera.h/.cpp`에 모여 있다. 카메라 입력 정책은 [Editor-Architecture.md](Editor-Architecture.md)에서 설명한다.

현재 ShowFlags는 `bPrimitive`, `bAxis`, `bGrid`다. Primitive 표시와 절두체 컬링 경로, Grid·Axis Pass가 구현되어 있다. Axis Pass는 World 원점부터 양의 X·Y·Z 방향으로 뻗는 무한 반직선을 각각 빨강·초록·파랑으로 표시한다.

### 가시 Primitive 수집과 Draw Command 수집

`CullView()`는 Opaque Node를 추가할 View마다 `VisiblePrimitives`를 비우고 Scene의 Proxy를 순회한다. SceneRenderer는 아직 업로드되지 않은 CPU Geometry를 먼저 업로드하고, Visible, 업로드 상태, 유효한 Bounds와 Frustum 교차를 검사한다. `bPrimitive`가 꺼져 있으면 Opaque Node를 추가하지 않는다.

가시성 결과는 패스의 입력이다. 향후 Shadow View나 다른 패스의 요구를 메인 View 가시성 하나로 대체하지 않는다.

## Draw Command

현재 `FMeshDrawCommand`는 Primitive Proxy, Mesh Buffer와 Material Resource의 비소유 포인터, Section index 범위와 `uint64` Sort Key를 담는다. Material Constant·Texture 배열을 Draw Command마다 복사하지 않는다. GPU 명령 버퍼 자체가 아니라 불투명 패스 실행에 필요한 임시 선택 정보다.

`VisiblePrimitives`는 SceneRenderer 멤버이며 View마다 다시 구성한다. `OpaqueCommands`는 `FOpaquePass::AddPass()`에서 생성되어 Opaque Node가 실행될 때까지 보관된다. 가시 Primitive 수만큼 reserve한 뒤 명령을 추가·정렬하며 View나 프레임 간 Draw Command 캐시는 없다.

```text
GetProxies → CullView → VisiblePrimitives
  → FOpaquePass::AddPass
      ├─ Renderer에서 Material Resource 생성·조회
      ├─ OpaqueCommands와 64비트 Sort Key 생성
      └─ Pipeline → Material → Mesh → Depth 정렬
  → Opaque Node 실행
      ├─ View 상수를 Pass당 한 번 바인딩
      ├─ 변경된 Pipeline·Material·Mesh 상태만 바인딩
      └─ Draw 상수 바인딩 → Draw
```

캐싱을 추가한다면 Mesh·Material·Pipeline 변경 시 무효화와 View 종속 데이터를 분리해야 한다. 배열을 멤버로 옮기는 것만으로 명령 캐시가 성립하지 않는다.

## Render Pass 관리

### 현재 Pass Node

`FOpaquePass`와 `FOverlayPass`는 장기 수명 인스턴스를 만들지 않는 정적 Node Builder다. 각 `AddPass()` 호출은 현재 View의 상수와 Draw 데이터를 캡처한 임시 Node를 생성한다. Node 실행 함수는 공용 Registry와 Cache에서 얻은 Handle을 바인딩하고 Draw를 수행한다. 범용 Render Pass 기반 클래스나 Pass registry는 없다.

`FShaderRegistry`는 소스 경로·Entry Point·Stage·Permutation ID로 구성된 `FShaderKey`별 GPU Shader를 최초 요청 시 생성한다. HLSL은 `Content/Engine/Shader`에서 읽으며 Win32 바이너리 리소스에 내장하지 않는다. Permutation ID를 컴파일 define에 적용하는 기능은 아직 구현하지 않는다. `FPipelineStateCache`는 완전한 `FPipelineStateDesc`별 PSO를 최초 요청 시 생성하고 Render Device 수명 동안 보관한다. Pass Node 파괴는 Shader나 PSO 수명에 영향을 주지 않으며 Overlay Node 내부에서는 Grid, Axis, Bounds를 고정 순서로 그린다.

Family 시작 시 Color·Depth 타깃을 바인딩하고 한 번 Clear한다. 각 Pass Node는 실행 직전에 자신의 Viewport를 설정한다. 각 View의 출력 영역은 Family 타깃 안에 있어야 한다. Family 종료 시 Back Buffer를 복구한다.

### 고정 Dependency Schedule

SceneRenderer는 ViewFamily마다 지역 `FRenderGraph`를 만들고 구체 Pass Builder를 고정 순서로 호출한다. Pass Builder는 Node만 등록하고, SceneRenderer가 반환된 raw `uint32` Node Index를 사용해 `AddDependency()`로 고정 실행 순서를 연결한다. Pass Builder는 자신의 입력, 명령 선택, Sort Key, 상수와 실행 함수를 책임지며 선행 Pass를 알지 않는다. `URenderer`는 구체 Pass를 모르고 완성된 Graph만 실행한다.

현재 구현된 순서는 `Opaque → Post Process Gamma Correction → Overlay`다. Post Process는 선형 Scene Color를 piecewise sRGB 전달 함수로 변환해 화면 표시용 Color에 기록한다. 조명, Exposure와 Tone Mapping은 적용하지 않는다. Grid, Axis, Bounds Show Flag가 모두 꺼지면 Overlay Node를 만들지 않으며, 활성화된 기능만 Node 내부에서 실행한다.

```text
목표 Forward 경로
Shadow → 필요 시 Depth Prepass → Opaque → Translucency → Post Process → Overlay

Deferred 경로를 선택할 경우
Shadow → 필요 시 Depth Prepass → Base/GBuffer → Light → Translucency → Post Process → Overlay
```

이 순서는 목표 의존 관계다. Light Pass나 GBuffer는 현재 구현이 아니며 모든 경로에 무조건 추가하는 것도 아니다. 전체 에디터 UI의 ImGui 합성은 ViewFamily 패스가 끝난 뒤 수행한다.

| 목표 Pass | 책임과 필요한 입력 |
|---|---|
| Shadow | Light별 View·Shadow Map, 별도 caster 컬링과 depth-only 실행 |
| Opaque / Base | 불투명 Mesh와 Material·Pipeline 선택, Forward 출력 또는 GBuffer 작성 |
| Light | Deferred 선택 시 GBuffer·Light·Shadow 결과로 조명 계산 |
| Translucency | 투명 Material 선택, 블렌딩과 뒤에서 앞으로의 정렬 |
| Optional Depth | 후속 패스의 깊이 재사용을 위한 선택적 prepass |
| Post Process | Scene Color를 입력으로 후처리와 출력 변환 |
| Overlay | Axis·Grid·선택 강조·기즈모 등 해당 View의 편집 보조 출력 |

## Pass별 정렬

현재 불투명 패스는 Draw Command 생성 시 `[Pipeline 12bit | Material 20bit | Mesh 20bit | Depth 12bit]`의 64비트 Sort Key를 만든다. Pipeline에는 Pipeline State Handle Index, Material에는 Material Resource Sort ID, Mesh에는 Vertex Buffer Handle Index를 사용한다. Depth는 WorldBounds 중심의 View 공간 Z를 Far Clip 범위로 정규화한다. 키는 `std::sort`로 오름차순 정렬하며, 실행 중 직전 상태를 추적해 동일한 Pipeline, Material binding과 Mesh Buffer를 다시 설정하지 않는다.

| 패스 | 현재 또는 목표 정렬 기준 |
|---|---|
| 현재 Opaque | Pipeline → Material → Mesh → Bounds 중심의 View depth |
| 목표 Opaque / Base | 장면의 CPU 상태 변경 비용과 GPU overdraw를 측정해 키 배치 조정 |
| 목표 Shadow | Shadow View 기준 depth와 depth Pipeline |
| 목표 Translucency | View 기준 back-to-front, 동일 깊이의 순서 정책 |
| 목표 Overlay | 레이어와 명시적 표시 순서 |

Bounds 중심 정렬은 교차하는 투명 Geometry의 정확한 합성을 보장하지 않는다. 투명 패스 구현 시 해당 한계와 정책을 별도로 결정한다.

## RHI와 백엔드 추상화

### 공통 계약

Engine은 Renderer 모듈의 구체 D3D11 타입을 참조하지 않는다. Editor가 구체 Device·Context·ImGui backend를 조립한다. API별 handle 해석과 native 자원 관리는 Renderer.dll 안에 둔다.

### IRenderDevice

Buffer·Texture·Shader·Pipeline State 생성과 제거, Command List 시작·종료·Submit, 타깃·Viewport·버퍼·상수 바인딩과 Draw를 제공한다. GPU 자원은 엔진 Handle로 참조하며 Handle의 존재만으로 자원 수명이 연장되지는 않는다.

### IRenderContext

네이티브 창에 연결되는 출력과 Swap Chain을 관리한다. Create·Release, BeginFrame·EndFrame, Resize, BindBackBuffer, Present와 출력 Viewport 조회를 제공한다.

### D3D11 백엔드

현재 D3D11 Immediate Context를 메인 스레드에서 사용한다. Command List 계약이 있다는 사실이 현재 명령을 다중 스레드에서 병행 기록한다는 의미는 아니다. 현재에는 Render Thread와 별도 RHI Thread가 없다.

### 목표 D3D12 백엔드

Command allocator/list 재사용, descriptor 관리, resource state transition, upload buffer 수명과 GPU Fence 기반 지연 해제가 필요하다. D3D11용 실행 코드를 함수명만 바꾸어 옮기는 것으로 완료되지 않는다. Scene·View 정책과 API별 자원 수명 처리는 분리한다.

## Texture와 Pipeline 계약

Viewport의 offscreen Scene Color는 선형 색을 저장하는 Render Target이자 Post Process 입력 Shader Resource다. 화면 표시용 Color는 Post Process 출력 Render Target이면서 ImGui에서 읽는 Shader Resource이고, Depth는 두 렌더 단계가 공유하는 DepthStencil 용도다. 현재 바인딩과 Clear는 Renderer가 수행하며 ImGui texture ID 변환은 GPU backend를 통해 처리한다.

Pipeline State는 Shader, Vertex Layout, Primitive Topology, Blend와 Depth 설정을 묶는다. Static Mesh는 `FStaticMeshVertex`와 Material이 선택한 Shader를 사용하며 ViewProjection은 View 상수 `b0`, Material Parameter는 Reflection이 지정한 슬롯, Model은 Draw 상수 `b3`으로 전달한다. `FPipelineStateCache`가 동일한 Description의 생성을 중복하지 않고, `FMaterialResource`가 해당 Pipeline과 Texture Resource·Sampler·상수 바인딩을 재사용한다.

### 상수 버퍼 슬롯 계약

Shader Stage별 상수 버퍼 슬롯은 데이터의 의미와 갱신 빈도에 따라 다음과 같이 고정한다. Shader가 사용하지 않는 슬롯은 바인딩할 필요가 없지만, 사용하는 상수는 해당 의미의 슬롯에 선언하고 Pass 실행 시점에 바인딩한다.

| 슬롯 | 의미 | 갱신 시점 |
|---|---|---|
| `b0` | View constants | View마다 한 번 |
| `b1` | Pass constants | Pass마다 한 번 |
| `b2` | Material constants | Material 변경 시 |
| `b3` | Draw/Object constants | Draw마다 |

현재 Opaque Pass는 Vertex Shader의 `b0`과 `b3`을 사용하고 Material Constant는 Shader Reflection Layout에 기록된 Stage와 Slot에 바인딩한다. 기본 Material Shader의 Material Constant는 `b2`를 사용한다. Overlay Pass의 Grid는 Pixel Shader의 `b0`과 `b1`, Axis는 Vertex·Pixel Shader의 `b0`을 사용한다.

## Material과 Pipeline 선택

Mesh/Material Render Command를 적용할 때 `FStaticMeshSceneProxy`가 Component Override와 Static Mesh Material Slot을 `FMaterialResource*` 배열로 변환한다. Opaque Pass는 Resource에 복사된 Section의 Material Index로 준비된 Material Resource를 조회하고 없으면 Renderer의 기본 Material Resource를 사용한다.

Material Resource는 Material의 Vertex/Pixel Shader, Cull·Depth·Blend 상태로 Pipeline State를 선택한다. Reflection Layout에 따라 상수를 패킹하고 Texture Resource와 Sampler Handle을 준비한다. Material이 없거나 유효하지 않으면 기본 Static Mesh Shader와 Magenta Base Color, 기본 White Texture를 사용한다.

현재는 Blend Mode가 Translucent인 Material도 별도 Translucency Pass가 아니라 Opaque Pass의 Pipeline State로 제출된다. Material 변경 무효화, 패스 참여 분리와 투명 객체의 back-to-front 정렬은 아직 구현하지 않았다.

Primitive 외의 Light나 다른 렌더 대상이 실제로 추가되면 해당 Proxy와 갱신 계약을 설계한다.

## Render Graph

현재 `FRenderGraph`는 한 ViewFamily 안에서만 존재하며 Pass Node, 실행 함수와 선행 Node Index 의존성을 보관한다. `URenderer::Execute()`가 의존성이 충족된 Node를 등록 순서에 안정적으로 실행하고 Graph는 실행 뒤 폐기된다. 별도 Render Graph Node Handle 타입은 두지 않는다.

현재 Graph는 Texture read/write 선언, 자동 resource barrier, transient resource aliasing이나 병렬 스케줄링을 제공하지 않는다. 이러한 기능은 패스 간 임시 타깃과 상태 전이 관리가 실제로 필요해질 때 raw Node Index 의존성 위에 추가한다.

## 스레딩과 수명

### 현재 Render Thread 계약

`FRenderThread`는 Renderer, Backend과 GPU Resource 생성·해제, Scene Command 적용, Draw·Submit·Present를 직렬 FIFO에서 실행한다. Game/Editor Thread는 Component에서 추출한 값과 ViewFamily만 제출하고 RT Proxy나 GPU Resource를 직접 수정하지 않는다.

Component의 OnUnregister는 Remove 명령을 제출하고 PrimitiveId를 초기화한다. World·Asset 종료 전에 Render Thread의 관련 명령을 완료하고 Renderer Asset Cache를 비운다.

ImGuiSystem은 Application을 비소유 참조로 보관하며 Startup에서 메시지 콜백을 등록하고 Shutdown에서 해제한다. Launch와 EditorEngine은 메시지를 ImGui로 중계하지 않는다. EditorEngine의 FImGuiSystem 직접 소유는 유지한다.

### 향후 GT/RT 중첩 경계

현재 Render Thread는 별도 Worker이지만 Frame Render는 `EnqueueAndWait()`로 완료를 기다린다. 향후 비동기 중첩 시에는 ViewFamily, ImGui Draw Data와 CPU Mesh/Texture 스냅샷이 RT 소비 완료까지 유지되어야 한다. Resize·World 종료·Asset 재임포트는 대기 중인 명령과 조율하고 CPU Command 완료와 GPU Fence 완료를 구분한다.

### GPU 자원 수명

CPU Proxy를 제거할 수 있는 시점과 GPU가 Mesh·Texture 사용을 끝내는 시점은 다르다. 현재 API 백엔드의 자원 관리 계약을 따르며, D3D12에서는 GPU 완료를 확인한 뒤 자원을 회수해야 한다. World·Proxy·Viewport 타깃과 UI backend는 RenderDevice보다 먼저 정리한다.

## 현재 구현 상태

### 구현됨

- World 소유 FScene과 지속적인 PrimitiveSceneProxy
- Component와 Proxy의 상호 참조를 제거한 PrimitiveId 기반 갱신
- Mesh·Visibility·Transform·부모 변경과 Inspector 편집 반영
- PrimitiveId별 Render Command 병합과 Render Thread Proxy 갱신
- Scene.GetProxies 기반 View별 Frustum Culling
- ViewFamily와 offscreen Color·Depth 타깃
- EditorEngine의 SceneRenderer 생성 및 Render(Renderer) 호출
- ViewFamily마다 생성되고 실행 뒤 폐기되는 raw Node Index 기반 Dependency Render Graph
- 임시 Opaque/Grid/Axis Node를 등록하는 상태 없는 Pass Builder
- Render Device 수명 동안 Shader와 PSO를 소유하는 `FShaderRegistry`, `FPipelineStateCache`
- Renderer 수명 동안 Material별 Pipeline·상수·Texture/Sampler binding을 재사용하는 `FMaterialResource`
- `FAssetId`와 Revision으로 관리되는 Static Mesh/Texture/Material Resource Cache
- Pipeline·Material·Mesh·Depth 64비트 키 기반 Opaque 정렬과 중복 상태 바인딩 생략
- D3D11 RHI, ImGui 출력 합성과 Submit·Present

### 미구현과 목표 순서

| 단계 | 목표 | 완료 기준 |
|---|---|---|
| Pass 확장 | Shadow 등 상태 없는 Node Builder와 Index 의존성 추가 | 현재 Opaque·Grid·Axis 출력 유지, Pass별 입력·출력·정렬 명확화 |
| Material·Light 확장 | Material 변경 무효화, 패스 참여 분리, Shadow·투명 등 추가 | View와 Pass별 명령 선택 및 정렬 검증 |
| GT/RT 프레임 중첩 | 현재 직렬 Render Thread 실행을 비동기 제출로 확장 | ImGui 깊은 복사와 최대 미완료 프레임 제한 |
| D3D12 | backend 및 GPU 완료 기반 자원 관리 | 자원 전이·재사용·지연 해제 검증 |
| Graph Resource 추적 | 자원 read/write, 상태 전이와 임시 타깃 관리 | 명시적 Node Index 의존성 위에서 자원 위험을 검증·해결 |

Pass 확장과 Render Thread 분리는 독립적인 변경으로 검증한다. Material과 모든 패스를 먼저 완성해야 Render Thread를 시작할 수 있다는 뜻은 아니다.

## 검증 기준

- 같은 PrimitiveId의 여러 부분 갱신은 다음 Render 전에 하나의 Command로 병합된다.
- Stopped·Paused·Playing의 World.Tick 뒤에 Scene 상태가 일관된다.
- 갱신 전후 PrimitiveId는 유지되고 Mesh Revision 변경 시 GPU Resource가 재생성된다.
- 부모 Transform 변경과 부모 삭제가 자손 Proxy에 반영된다.
- Add, Update, Remove 명령이 FIFO 순서로 적용되고 World 종료 후 Proxy와 Asset 참조가 남지 않는다.
- 여러 View는 독립적으로 컬링하고 Family 타깃은 한 번만 Clear한다.
- World 상태 갱신 횟수는 ViewFamily 개수와 무관하다.
- 불투명 정렬·Draw와 ImGui 합성 결과를 실제 D3D11 출력으로 확인한다.

이 목록은 검증 계약이며 모든 항목이 저장소의 자동 테스트로 구현되어 있다는 뜻은 아니다.

## 관련 파일

- [World.cpp](../KnotEngine/Source/Engine/World/World.cpp)
- [PrimitiveComponent.h](../KnotEngine/Source/Engine/Component/PrimitiveComponent.h)
- [StaticMeshComponent.cpp](../KnotEngine/Source/Engine/Component/Mesh/StaticMeshComponent.cpp)
- [StaticMesh.cpp](../KnotEngine/Source/Engine/Asset/Mesh/StaticMesh.cpp)
- [GeometryMesh.cpp](../KnotEngine/Source/Engine/Render/Resource/Mesh/GeometryMesh.cpp)
- [StaticMeshResource.cpp](../KnotEngine/Source/Engine/Render/Resource/Mesh/StaticMeshResource.cpp)
- [PrimitiveSceneProxy.h](../KnotEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.h)
- [PrimitiveSceneProxy.cpp](../KnotEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.cpp)
- [MaterialResource.h](../KnotEngine/Source/Engine/Render/Resource/MaterialResource.h)
- [MaterialResource.cpp](../KnotEngine/Source/Engine/Render/Resource/MaterialResource.cpp)
- [TextureResource.h](../KnotEngine/Source/Engine/Render/Resource/TextureResource.h)
- [Scene.h](../KnotEngine/Source/Engine/Render/Scene/Scene.h)
- [SceneView.h](../KnotEngine/Source/Engine/Render/Scene/SceneView.h)
- [SceneRenderer.cpp](../KnotEngine/Source/Engine/Render/Scene/SceneRenderer.cpp)
- [OpaquePass.cpp](../KnotEngine/Source/Engine/Render/Pass/OpaquePass.cpp)
- [Renderer.cpp](../KnotEngine/Source/Engine/Render/Renderer.cpp)
- [RenderSystem.cpp](../KnotEngine/Source/Renderer/Render/RenderSystem.cpp)
- [RenderThread.cpp](../KnotEngine/Source/Renderer/Render/RenderThread.cpp)
- [RenderDevice.h](../KnotEngine/Source/Engine/Render/RHI/RenderDevice.h)
- [RenderContext.h](../KnotEngine/Source/Engine/Render/RHI/RenderContext.h)
- [EditorViewportCamera.h](../KnotEngine/Source/Editor/Viewport/EditorViewportCamera.h)
- [EditorViewportClient.cpp](../KnotEngine/Source/Editor/Viewport/EditorViewportClient.cpp)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/Runtime/EditorEngine.cpp)
- [ImGuiSystem.cpp](../KnotEngine/Source/Editor/Editor/ImGuiSystem.cpp)
- [Conventions.md](Conventions.md)
