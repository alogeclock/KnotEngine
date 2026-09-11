# Knot Engine Rendering Architecture

## 문서 목적

이 문서는 Knot Engine의 World 렌더 상태, ViewFamily 구성, 가시성 판정, Draw Command 생성과 GPU 실행의 책임을 정의한다. 현재 구현은 메인 스레드에서 동기 실행하는 D3D11 렌더링 경로다.

Render Thread 분리와 Render Pass 객체화는 향후 목표다. 현재 코드의 실행 경로와 목표 구조를 구분하며, 목표 타입과 패스가 이미 존재하는 것으로 해석하지 않는다. 문서의 구성은 [Input-Architecture.md](Input-Architecture.md)와 같은 목적·원칙·전체 구조·세부 계약·구현 상태 순서를 따른다.

## 설계 원칙

- World는 게임 상태와 Scene의 수명을 관리하고 World 갱신 끝에서 Scene을 갱신한다.
- FScene은 Proxy를 직접 소유하며 Component 목록이나 별도 등록 정보 구조체를 보관하지 않는다.
- Component는 자신의 Proxy를 Dirty로 표시하고, Proxy가 필요한 데이터를 원본 Component에서 읽는다.
- 현재 Component와 Proxy의 상호 참조는 단일 스레드와 등록 수명 안에서만 유효하다.
- ViewportClient는 카메라와 출력 영역으로 View 및 ViewFamily를 구성한다.
- EditorEngine은 프레임을 조율하고 ViewFamily마다 SceneRenderer를 생성한다.
- SceneRenderer는 가시성·명령 선택·패스 순서를 결정하고 Renderer는 렌더 명령을 실행한다.
- 가시 Primitive 수집과 패스별 Draw Command 생성은 별도 단계다.
- 패스마다 목적에 맞는 Sort Key를 사용한다. Scene 전체를 하나의 정렬 순서로 고정하지 않는다.
- RHI 계약은 Engine.dll에, D3D11 구현은 Renderer.dll에 둔다.
- ImGui 화면 구성은 Editor, ImGui GPU 백엔드는 Renderer 모듈의 책임이다.
- 최초 구현에서는 실제 DLL 경계 외의 범용 인터페이스와 Factory를 추가하지 않는다.

## 전체 구조

```text
UEditorEngine
    ├─ WorldContext별 UWorld::Tick
    │      ├─ Playing: Level → Node → Component::TickComponent
    │      └─ 모든 PlayState: FScene::UpdatePrimitiveSceneProxies
    │             └─ FPrimitiveSceneProxy::Update
    │                    └─ Dirty이면 Component 상태 복사
    ├─ ViewportClient Tick / BuildSceneViewFamily
    │      └─ BuildSceneView
    └─ Render
           ├─ URenderer::BeginFrame
           ├─ Family별 FSceneRenderer 생성
           │      └─ Render(Renderer)
           │             ├─ View별 CullView
           │             └─ RenderOpaquePass → Sort → Draw
           ├─ FImGuiSystem::Render
           └─ URenderer::EndFrame → Submit → Present
```

```text
UWorld ──소유──> FScene ──소유──> FPrimitiveSceneProxy[]
                                   ↕ 비소유 참조
                           UMeshComponent

FSceneViewFamily ──참조──> FScene
                ├─ FSceneView[]
                ├─ FSceneRenderTarget
                └─ FShowFlags
```

## 디렉터리와 책임

```text
KnotEngine/Source/
├─ Engine/
│  ├─ Component/
│  │  ├─ PrimitiveComponent.h/.cpp
│  │  └─ MeshComponent.h/.cpp
│  ├─ World/World.h/.cpp
│  └─ Render/
│     ├─ Renderer.h/.cpp
│     ├─ Scene/
│     │  ├─ Scene.h/.cpp
│     │  ├─ SceneView.h
│     │  └─ SceneRenderer.h/.cpp
│     ├─ Proxy/PrimitiveSceneProxy.h/.cpp
│     ├─ Resource/
│     │  ├─ Buffer.h/.cpp
│     │  ├─ MeshTypes.h/.cpp
│     │  ├─ MeshResources.h/.cpp
│     │  └─ VertexTypes.h/.cpp
│     └─ RHI/
│        ├─ RenderDevice.h
│        ├─ RenderContext.h
│        ├─ RenderTypes.h
│        └─ VertexLayout.h
├─ Renderer/Render/
│  ├─ D3D11/
│  └─ ImGui/
└─ Editor/
   ├─ Runtime/EditorEngine.h/.cpp
   ├─ Viewport/
   │  ├─ Viewport.h/.cpp
   │  ├─ EditorViewportCamera.h/.cpp
   │  └─ EditorViewportClient.h/.cpp
   └─ ImGui/ImGuiSystem.h/.cpp
```

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `UWorld` | Level·Component 수명, Tick, Scene 소유와 갱신 시점 | Dirty Component 목록, 패스 선택 |
| `UPrimitiveComponent` | Proxy 생성 계약, 등록·해제, Dirty 표시 | GPU 명령 실행 |
| `FPrimitiveSceneProxy` | 원본 상태 복사, WorldMatrix·Bounds·Mesh 보관 | View별 컬링, 명령 정렬 |
| `FScene` | Proxy 소유·순회·제거, `GetProxies()` 제공 | Component 데이터 접근 |
| `FEditorViewportClient` | 카메라 입력, View·ViewFamily 구성 | SceneRenderer 생성, Draw 실행 |
| `FSceneRenderer` | View별 가시성, 패스별 명령과 순서 | World Tick, Proxy 갱신, Present |
| `URenderer` | 공용 GPU 자원, 프레임, 타깃·상수·Mesh 실행 | SceneRenderer 생성, 가시성 정책 |
| `IRenderDevice` | GPU 자원과 Command List 연산 | World·Editor 정책 |
| `IRenderContext` | 네이티브 창 출력, Swap Chain, Resize·Present | Scene 순회 |
| `UEditorEngine` | 렌더 작업 생성과 UI 합성 순서 | 패스 내부 컬링·정렬 |

`URenderer`는 이름에 U 접두사가 있지만 현재 UObject를 상속하지 않는 일반 C++ 객체다. `FSceneInterface`, `IRendererModule` 등의 추가 추상 계층은 없다.

## 프레임 실행 순서

### 현재 구현

1. ImGui 프레임과 패널을 구성하고 입력을 라우팅한다.
2. EditorEngine이 각 WorldContext의 World를 한 번 Tick한다.
3. World는 Playing 상태에서 Level과 Component를 Tick하고, 모든 PlayState에서 마지막에 Scene을 갱신한다.
4. 각 Proxy는 Dirty 여부를 검사한다. Dirty가 아니면 반환하고, Dirty이면 원본 상태를 복사한 뒤 플래그를 해제한다.
5. ViewportClient의 카메라를 Tick하고 ImGui draw data를 확정한다.
6. 출력 가능한 Client에서 ViewFamily를 구성한다.
7. Renderer 프레임을 시작하고 Family마다 지역 SceneRenderer를 생성하여 실행한다.
8. 모든 offscreen 결과를 포함한 ImGui draw data를 Back Buffer에 합성한다.
9. Command List를 종료·제출하고 Present한다.

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
    → Render Pass 객체별 명령 구성·정렬·기록
    → 준비된 UI 출력 합성 → Submit / Present
```

상위 엔진이 렌더 요청을 조율하는 책임은 유지한다. 비동기 작업의 구체적인 SceneRenderer 생성 위치와 프레임 데이터 소유 타입은 Render Thread 구현 시 정한다. 현재의 스택 객체와 ViewFamily 참조를 그대로 다른 스레드에 넘기지 않는다.

## Scene 렌더 데이터

### FScene

`FScene`은 `TArray<std::unique_ptr<FPrimitiveSceneProxy>>`를 소유한다. `AddPrimitive()`는 소유권을 받고 안정적인 Proxy 참조를 반환한다. `RemovePrimitive()`는 해당 Proxy를 찾아 즉시 파괴한다. 배열 재할당은 unique_ptr을 이동하며 Proxy의 힙 주소는 유지한다.

`UpdatePrimitiveSceneProxies()`는 각 Proxy의 `Update()`를 호출한다. Component를 조회하거나 원본 데이터를 읽는 코드는 Scene에 없다. SceneRenderer도 `GetProxies()`로 갱신된 렌더 상태만 조회한다.

### Component와 Proxy

```text
OnRegister
  → 가상 CreatePrimitiveSceneProxy()
  → MeshComponent가 Proxy 생성 및 초기 상태 구성
  → FScene.AddPrimitive

Transform / Mesh / Visibility / Inspector 변경
  → MarkPrimitiveSceneProxy()
  → SceneProxy->bDirty = true

World.Tick 끝
  → Scene.UpdatePrimitiveSceneProxies()
  → Proxy.Update()
  → Dirty이면 Component 상태 복사 및 Bounds 계산

OnUnregister
  → Scene.RemovePrimitive
  → Component의 SceneProxy = nullptr
```

`UComponent`에는 가상 `Update()`가 없다. `CreatePrimitiveSceneProxy()`는 PrimitiveComponent의 순수 가상 함수이고 현재 MeshComponent에서 구현한다. 매 갱신마다 Proxy를 새로 만들지 않는다.

`FPrimitiveSceneProxy`는 현재 `const UMeshComponent&`를 보관한다. 범용 Primitive나 Light Proxy 계층으로 일반화된 상태는 아니다. PrimitiveComponent·MeshComponent와 Proxy는 서로 friend로 선언된다. Proxy는 Mesh와 Visibility를 직접 읽고 Transform의 World Matrix를 조회한다.

| Proxy 데이터 | 의미 |
|---|---|
| `Mesh` | 렌더링에 사용할 Geometry Mesh 공유 참조 |
| `WorldMatrix` | 로컬 공간에서 월드 공간으로 변환 |
| `LocalBounds` | Mesh의 로컬 AABB |
| `WorldBounds` | WorldMatrix를 적용한 AABB |
| `bVisible` | View와 무관한 표시 상태 |
| `bDirty` | 원본 Component에서 다시 복사할 필요 여부 |
| `Component` | 등록 수명 안에서 유효한 비소유 원본 참조 |

Mesh가 없거나 Mesh Buffer가 유효하지 않으면 렌더용 Mesh 참조와 Bounds를 비운다. Mesh가 없는 Component도 Proxy 등록은 유지한다. 공유 Mesh 데이터를 직접 수정한 경우 업로드 후 관련 Component를 명시적으로 Mark해야 하며 Asset 변경 구독은 아직 없다.

부모 Transform 변경은 자손까지 Dirty를 전파한다. Stopped·Paused에서도 Scene 갱신을 실행하므로 Inspector 편집 결과가 반영된다. World Tick 이후 변경한 값은 다음 Scene 갱신에서 반영된다.

### View와 ViewFamily

`FSceneView`는 ViewMatrix, ProjectionMatrix, ViewProjectionMatrix, ViewOrigin, Frustum과 FRenderViewport를 보관한다. 행벡터 규약으로 `ViewProjectionMatrix = ViewMatrix * ProjectionMatrix`이며 Draw 시 `WorldMatrix * ViewProjectionMatrix`를 사용한다.

`FSceneViewFamily`는 Scene 참조, View 배열, RenderTarget과 ShowFlags를 묶는다. `FSceneRenderTarget`은 Color·Depth texture handle과 Width·Height를 보관한다. Family는 Scene이나 GPU 타깃을 소유하지 않는다.

`FEditorViewportClient::GetWorld()`는 기본적으로 `GEngine->GetWorld()`를 사용하고 파생 Client가 다른 World를 선택할 수 있다. `BuildSceneViewFamily()`는 출력과 Scene이 유효할 때 `BuildSceneView()` 결과를 묶는다. 현재 Editor는 Family당 View 하나를 만들고 SceneRenderer는 여러 View를 처리할 수 있다.

`FEditorViewportCameraTransform`, `EEditorViewportViewMode`, `FEditorViewportCamera`는 `EditorViewportCamera.h/.cpp`에 모여 있다. 카메라 입력 정책은 [Editor-Architecture.md](Editor-Architecture.md)에서 설명한다.

현재 ShowFlags는 `bPrimitive`, `bAxis`, `bGrid`다. Primitive 표시와 절두체 컬링 경로만 구현되어 있고 Axis·Grid 패스는 아직 없다.

### 가시 Primitive 수집과 Draw Command 수집

`CullView()`는 View마다 `VisiblePrimitives`를 비우고 Scene의 Proxy를 순회한다. Visible, Mesh Buffer 존재, 유효한 Bounds와 Frustum 교차를 검사한다. `bPrimitive`가 꺼져 있으면 결과가 비어 있다.

가시성 결과는 패스의 입력이다. 향후 Shadow View나 다른 패스의 요구를 메인 View 가시성 하나로 대체하지 않는다.

## Draw Command

현재 `FMeshDrawCommand`는 Proxy의 비소유 포인터와 uint32 SortKey만 담는다. GPU 명령 버퍼 자체가 아니라 불투명 패스 실행에 필요한 임시 선택 정보다.

`VisiblePrimitives`는 SceneRenderer 멤버이며 View마다 다시 구성한다. `OpaqueCommands`는 `RenderOpaquePass()`의 지역 배열이며 가시 Primitive 수만큼 reserve한 뒤 명령을 추가·정렬·실행한다. View나 프레임 간 Draw Command 캐시는 없다.

```text
GetProxies → CullView → VisiblePrimitives
  → RenderOpaquePass의 OpaqueCommands
  → stable_sort → UpdateConstant → DrawMeshBuffer
```

캐싱을 추가한다면 Mesh·Material·Pipeline 변경 시 무효화와 View 종속 데이터를 분리해야 한다. 배열을 멤버로 옮기는 것만으로 명령 캐시가 성립하지 않는다.

## Render Pass 관리

### 현재 불투명 패스

현재는 SceneRenderer의 `RenderOpaquePass()` 함수 하나가 명령 선택·정렬·실행을 담당한다. 별도 Render Pass 객체나 Pass registry는 없다. 모든 Geometry가 Common shader와 하나의 Graphics Pipeline을 사용한다.

Family 시작 시 Color·Depth 타깃을 바인딩하고 한 번 Clear한다. 이후 각 View 직전에 `RenderDevice.SetViewport(CommandList, View.Viewport)`를 실행한다. 각 View의 출력 영역은 Family 타깃 안에 있어야 한다. Family 종료 시 Back Buffer를 복구한다.

### 목표: Render Pass 객체화와 고정 스케줄

패스가 늘어나면 SceneRenderer가 구체적인 Pass 객체를 조율하도록 분리한다. Pass는 자신의 입력·출력, 명령 선택, Sort Key와 실행을 책임진다. 처음에는 구체 클래스와 고정 호출 순서로 구현하고 범용 기반 클래스나 동적 등록 체계는 실제 공통 계약이 확인될 때 도입한다.

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

현재 불투명 패스는 WorldBounds 중심의 View 공간 Z를 사용한다. NaN과 무한대를 검사하고 음수 깊이를 0으로 제한한 뒤 float 비트를 uint32로 해석한다. 비음수 유한 float 범위에서 이 키를 오름차순 stable sort하여 앞에서 뒤로 그린다.

| 패스 | 현재 또는 목표 정렬 기준 |
|---|---|
| 현재 Opaque | Bounds 중심의 View depth, front-to-back |
| 목표 Opaque / Base | Pipeline·Material 변경 비용과 depth를 고려한 키 |
| 목표 Shadow | Shadow View 기준 depth와 depth Pipeline |
| 목표 Translucency | View 기준 back-to-front, 동일 깊이의 순서 정책 |
| 목표 Overlay | 레이어와 명시적 표시 순서 |

Bounds 중심 정렬은 교차하는 투명 Geometry의 정확한 합성을 보장하지 않는다. 투명 패스 구현 시 해당 한계와 정책을 별도로 결정한다.

## RHI와 백엔드 추상화

### 공통 계약

Engine은 Renderer 모듈의 구체 D3D11 타입을 참조하지 않는다. Editor가 구체 Device·Context·ImGui backend를 조립한다. API별 handle 해석과 native 자원 관리는 Renderer.dll 안에 둔다.

### IRenderDevice

Buffer·Texture·Shader·Graphics Pipeline 생성과 제거, Command List 시작·종료·Submit, 타깃·Viewport·버퍼·상수 바인딩과 Draw를 제공한다. GPU 자원은 엔진 Handle로 참조하며 Handle의 존재만으로 자원 수명이 연장되지는 않는다.

### IRenderContext

네이티브 창에 연결되는 출력과 Swap Chain을 관리한다. Create·Release, BeginFrame·EndFrame, Resize, BindBackBuffer, Present와 출력 Viewport 조회를 제공한다.

### D3D11 백엔드

현재 D3D11 Immediate Context를 메인 스레드에서 사용한다. Command List 계약이 있다는 사실이 현재 명령을 다중 스레드에서 병행 기록한다는 의미는 아니다. 현재에는 Render Thread와 별도 RHI Thread가 없다.

### 목표 D3D12 백엔드

Command allocator/list 재사용, descriptor 관리, resource state transition, upload buffer 수명과 GPU Fence 기반 지연 해제가 필요하다. D3D11용 실행 코드를 함수명만 바꾸어 옮기는 것으로 완료되지 않는다. Scene·View 정책과 API별 자원 수명 처리는 분리한다.

## Texture와 Pipeline 계약

Viewport의 offscreen Color는 Render Target이면서 ImGui에서 읽는 Shader Resource이고 Depth는 DepthStencil 용도다. 현재 바인딩과 Clear는 Renderer가 수행하며 ImGui texture ID 변환은 GPU backend를 통해 처리한다.

Graphics Pipeline은 Shader, Vertex Layout, Primitive Topology와 depth 설정을 묶는다. 현재 FGeometryVertex의 위치·색상 데이터와 Common shader를 사용하며 WorldViewProjection을 Vertex Shader 상수로 전달한다. Material별 texture·sampler·상수와 여러 Pipeline 조합은 후속 확장이다.

## Material과 Pipeline 선택

현재 Proxy는 Geometry Mesh를 보관하며 Material 시스템은 연결되지 않았다. 향후 Material의 Blend Mode와 패스 참여 조건으로 Shader·Pipeline·리소스 바인딩을 선택한다. Component가 GPU 상태를 직접 설정하거나 Scene이 모든 패스의 명령을 미리 하나로 정렬하지 않는다.

Primitive 외의 Light나 다른 렌더 대상이 실제로 추가되면 해당 Proxy와 갱신 계약을 설계한다. 현재 `UMeshComponent&`를 받는 Proxy를 이미 범용 Proxy 계층인 것처럼 취급하지 않는다.

## Render Graph 도입 기준

Render Pass 객체화와 Render Graph 도입은 별개다. 우선 고정 스케줄에서 Pass의 책임을 분리한다. 임시 타깃 수명, 패스 간 읽기·쓰기 의존성, 상태 전이와 자원 재사용 관리가 복잡해질 때 Graph를 검토한다. 현재 Graph나 병렬 패스 스케줄러는 없다.

## 스레딩과 수명

### 현재 단일 스레드 계약

World 변경, Proxy 복사, UI 구성과 GPU 호출은 같은 메인 스레드에서 순서대로 진행한다. Proxy는 원본 Component를 직접 읽을 수 있지만 SceneRenderer 실행 중에는 Component나 Proxy를 변경·제거하지 않는다.

Component의 OnUnregister가 Scene에서 Proxy를 제거한 뒤 Component를 파괴한다. World는 Level·Component를 먼저 파괴하고 마지막에 Scene을 소멸시킨다. ViewFamily와 Draw Command의 비소유 참조는 동기 Render 호출이 끝날 때까지 유효해야 한다.

ImGuiSystem은 Application을 비소유 참조로 보관하며 Startup에서 메시지 콜백을 등록하고 Shutdown에서 해제한다. Launch와 EditorEngine은 메시지를 ImGui로 중계하지 않는다. EditorEngine의 FImGuiSystem 직접 소유는 유지한다.

### 목표 Render Thread 경계

현재의 friend 관계는 스레드 동기화를 제공하지 않는다. Render Thread가 현재 Proxy.Update()를 그대로 실행하여 게임 스레드의 Component를 읽게 해서는 안 된다.

Render Thread 분리 시 다음 계약을 먼저 구현한다.

1. World 갱신 끝에서 Game Thread가 Dirty Component의 렌더 데이터를 복사한다.
2. 전달 데이터는 스레드 간 읽기 중 변경되지 않으며 원본 Component 수명에 의존하지 않는다.
3. Render Thread가 자신의 Scene 상태에 등록·갱신·제거를 순서대로 적용한다.
4. Render Thread의 Proxy는 Component를 역참조하지 않는다. 현재 원본 참조와 Dirty 처리 경계는 이때 재설계한다.
5. ViewFamily·Render Target·Mesh 자원과 ImGui draw data를 소비 완료까지 유지한다. 다음 UI 프레임과의 중첩에는 draw data 복사 또는 명시적 동기화가 필요하다.
6. Resize·World 종료·타깃 제거는 대기 중인 렌더 작업과 조율한다. CPU 작업 완료와 GPU Fence 완료를 구분한다.

먼저 단일 Render Thread를 분리하고 동기 버전과 결과를 비교한다. 다중 워커의 병렬 컬링·Draw Command 생성·패스 기록과 별도 RHI Thread는 현재 목표 구현 범위에 포함하지 않는다.

### GPU 자원 수명

CPU Proxy를 제거할 수 있는 시점과 GPU가 Mesh·Texture 사용을 끝내는 시점은 다르다. 현재 API 백엔드의 자원 관리 계약을 따르며, D3D12에서는 GPU 완료를 확인한 뒤 자원을 회수해야 한다. World·Proxy·Viewport 타깃과 UI backend는 RenderDevice보다 먼저 정리한다.

## 현재 구현 상태

### 구현됨

- World 소유 FScene과 지속적인 PrimitiveSceneProxy
- Component와 Proxy의 friend 접근 및 비소유 참조
- Proxy 자체 Dirty 표시, World.Tick 끝의 조건부 데이터 갱신
- Mesh·Visibility·Transform·부모 변경과 Inspector 편집 반영
- Scene.GetProxies 기반 View별 Frustum Culling
- ViewFamily와 offscreen Color·Depth 타깃
- EditorEngine의 SceneRenderer 생성 및 Render(Renderer) 호출
- 단일 불투명 함수 패스와 지역 OpaqueCommands 정렬·실행
- D3D11 RHI, ImGui 출력 합성과 Submit·Present

### 미구현과 목표 순서

| 단계 | 목표 | 완료 기준 |
|---|---|---|
| Pass 객체화 | 구체 Pass로 책임 분리, 고정 스케줄 | 현재 불투명 출력 유지, Pass별 입력·출력·정렬 명확화 |
| Material·Light 확장 | 패스 참여와 Pipeline 선택, Shadow·투명 등 추가 | View와 Pass별 명령 선택 및 정렬 검증 |
| Render Thread | 전달 데이터와 렌더 상태 소유 분리 | Component 파괴·Resize·UI 수명과 프레임 순서 검증 |
| D3D12 | backend 및 GPU 완료 기반 자원 관리 | 자원 전이·재사용·지연 해제 검증 |
| 필요 시 Render Graph | 자원 의존성과 임시 타깃 관리 | 고정 스케줄보다 관리해야 할 복잡성을 실제로 줄임 |

Pass 객체화와 Render Thread 분리는 독립적인 변경으로 검증한다. Material과 모든 패스를 먼저 완성해야 Render Thread를 시작할 수 있다는 뜻은 아니다.

## 검증 기준

- Clean Proxy는 상태를 다시 복사하지 않고 여러 Mark는 다음 갱신 한 번으로 합쳐진다.
- Stopped·Paused·Playing의 World.Tick 뒤에 Scene 상태가 일관된다.
- 갱신 전후 Proxy 주소는 유지되고 Mesh 해제 시 Bounds도 비워진다.
- 부모 Transform 변경과 부모 삭제가 자손 Proxy에 반영된다.
- Dirty 상태에서 등록 해제·재등록·World 종료해도 원본 참조가 남지 않는다.
- 여러 View는 독립적으로 컬링하고 Family 타깃은 한 번만 Clear한다.
- World 상태 갱신 횟수는 ViewFamily 개수와 무관하다.
- 불투명 정렬·Draw와 ImGui 합성 결과를 실제 D3D11 출력으로 확인한다.

이 목록은 검증 계약이며 모든 항목이 저장소의 자동 테스트로 구현되어 있다는 뜻은 아니다.

## 관련 파일

- [World.cpp](../KnotEngine/Source/Engine/World/World.cpp)
- [PrimitiveComponent.h](../KnotEngine/Source/Engine/Component/PrimitiveComponent.h)
- [MeshComponent.cpp](../KnotEngine/Source/Engine/Component/MeshComponent.cpp)
- [PrimitiveSceneProxy.h](../KnotEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.h)
- [PrimitiveSceneProxy.cpp](../KnotEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.cpp)
- [Scene.h](../KnotEngine/Source/Engine/Render/Scene/Scene.h)
- [SceneView.h](../KnotEngine/Source/Engine/Render/Scene/SceneView.h)
- [SceneRenderer.cpp](../KnotEngine/Source/Engine/Render/Scene/SceneRenderer.cpp)
- [Renderer.cpp](../KnotEngine/Source/Engine/Render/Renderer.cpp)
- [RenderDevice.h](../KnotEngine/Source/Engine/Render/RHI/RenderDevice.h)
- [RenderContext.h](../KnotEngine/Source/Engine/Render/RHI/RenderContext.h)
- [EditorViewportCamera.h](../KnotEngine/Source/Editor/Viewport/EditorViewportCamera.h)
- [EditorViewportClient.cpp](../KnotEngine/Source/Editor/Viewport/EditorViewportClient.cpp)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/Runtime/EditorEngine.cpp)
- [ImGuiSystem.cpp](../KnotEngine/Source/Editor/ImGui/ImGuiSystem.cpp)
- [Conventions.md](Conventions.md)
