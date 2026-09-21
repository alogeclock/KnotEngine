# Knot Engine Rendering Architecture

## 문서 목적

이 문서는 Knot Engine의 Game/Editor Thread 렌더 상태 추출, Render Thread 제출, Scene Proxy와 Asset Resource 관리, View별 컬링, Render Graph Pass 구성 및 D3D11 실행 경계를 정의한다.

현재 렌더링은 전용 `FRenderThread`에서 실행한다. Component와 Asset은 GPU 객체를 직접 소유하거나 수정하지 않고, 값으로 구성된 Scene Command와 Resource Command를 제출한다. Render Thread는 이 명령을 적용한 뒤 ViewFamily마다 Render Graph를 만들고 Opaque, Selection, Post Process, Overlay와 Debug Draw를 실행한다.

## 설계 원칙

- World는 게임 객체와 `FScene`의 수명을 관리한다.
- Component는 Proxy 주소 대신 `PrimitiveId`만 보관한다.
- Component 변경은 `Transform`, `Mesh`, `Material`, `Visibility` 값 명령으로 전달한다.
- 같은 프레임의 동일한 `PrimitiveId` 갱신은 Game Thread 제출 큐에서 병합한다.
- Scene Proxy는 Component와 UObject를 참조하지 않는다.
- Asset의 CPU 데이터와 GPU Resource 갱신을 Primitive Command에 반복해서 복사하지 않는다.
- Static Mesh, Texture, Material은 Asset별 Resource Command로 한 Revision당 한 번 전송한다.
- Primitive Command는 Asset을 `FAssetId`로 연결하고 Render Thread에서 Resource Cache를 조회한다.
- View별 가시성 판정과 Pass별 Draw Command 생성은 분리한다.
- 같은 Mesh LOD와 Material 구성을 공유하는 Primitive는 일정 수 이상일 때 자동으로 인스턴싱한다.
- Shader, Pipeline, Sampler와 Asset Resource는 Draw Pass 밖에서 준비하고 Renderer 수명 동안 재사용한다.
- Render Graph는 현재 프레임의 실행 순서만 표현하며 GPU Resource 수명이나 상태 전이를 자동 관리하지 않는다.
- RHI 계약은 Engine 모듈에, D3D11 구현과 Render Thread 소유권은 Renderer 모듈에 둔다.
- D3D11 Immediate Context와 GPU Resource 생성·파괴는 Render Thread에서만 수행한다.
- 최초 구현에서는 실제 DLL 경계 밖의 범용 Factory나 Pass 기반 클래스처럼 불필요한 추상화를 추가하지 않는다.

## 전체 구조

```text
Game / Editor Thread
    ├─ UWorld::Tick
    ├─ Component 변경
    │   └─ FScene Pending Primitive Commands
    ├─ Asset Resource 요청
    │   └─ FAssetManager Pending Resource Commands
    ├─ FSceneViewFamily 구성
    ├─ ImGui Draw Data 깊은 복사
    └─ FRenderSystem::Render
            ↓ 최대 2 Frames In Flight
Render Thread
    ├─ Texture Resource Commands 적용
    ├─ Static Mesh Resource Commands 적용
    ├─ Material Resource Commands 적용
    ├─ Scene Primitive Commands 적용
    ├─ FRenderer::BeginFrame
    ├─ ViewFamily별 FSceneRenderer::Render
    │   ├─ Frustum Culling
    │   ├─ Render Graph 구성
    │   └─ Pass 실행
    ├─ 복사된 ImGui Draw Data 합성
    └─ FRenderer::EndFrame
        └─ Submit → Present
```

```text
UPrimitiveComponent [GT]
    └─ PrimitiveId + FPrimitiveRenderData
            ↓
FScene Pending Commands [GT]
            ↓ Drain
FScene Proxy Storage [RT]
    └─ FStaticMeshSceneProxy
        ├─ World Transform / Bounds / Visibility
        ├─ FStaticMeshResource*
        └─ FMaterialResource*[]

FAssetManager [GT]
    └─ AssetId + Revision + CPU 데이터
            ↓ Resource Commands
FRenderer Resource Cache [RT]
    ├─ FStaticMeshResource[]
    ├─ FTextureResource[]
    └─ FMaterialResource[]
```

## 디렉터리와 책임

```text
KnotEngine/Content/Engine/Shader/
├─ StaticMesh.hlsl
├─ Selection.hlsl
├─ PostProcess.hlsl
├─ DebugDraw.hlsl
├─ Grid.hlsl
└─ Axis.hlsl

KnotEngine/Source/
├─ Engine/
│  ├─ Component/
│  │  ├─ PrimitiveComponent.h/.cpp
│  │  └─ Mesh/StaticMeshComponent.h/.cpp
│  ├─ Asset/AssetManager.h/.cpp
│  └─ Render/
│     ├─ Renderer.h/.cpp
│     ├─ Graph/RenderGraph.h/.cpp
│     ├─ Pass/
│     │  ├─ OpaquePass.h/.cpp
│     │  ├─ SelectionPass.h/.cpp
│     │  ├─ PostProcessPass.h/.cpp
│     │  ├─ OverlayPass.h/.cpp
│     │  └─ DebugDrawPass.h/.cpp
│     ├─ Proxy/PrimitiveSceneProxy.h/.cpp
│     ├─ Scene/
│     │  ├─ RenderCommand.h
│     │  ├─ Scene.h/.cpp
│     │  ├─ SceneView.h
│     │  └─ SceneRenderer.h/.cpp
│     ├─ Resource/
│     │  ├─ ResourceCommand.h
│     │  ├─ MaterialResource.h/.cpp
│     │  ├─ TextureResource.h/.cpp
│     │  ├─ Mesh/
│     │  └─ State/
│     ├─ Shader/
│     └─ RHI/
├─ Renderer/Render/
│  ├─ RenderSystem.h/.cpp
│  ├─ RenderThread.h/.cpp
│  ├─ RenderBackend.h/.cpp
│  ├─ D3D11/
│  └─ ImGui/
└─ Editor/
   ├─ Runtime/EditorEngine.h/.cpp
   ├─ Viewport/
   └─ Editor/ImGuiSystem.h/.cpp
```

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `UWorld` | Level·Component와 Scene 수명 관리, Tick | GPU Resource와 Draw 실행 |
| `UPrimitiveComponent` | Primitive 상태 추출, PrimitiveId 보관, 부분 명령 제출 | Proxy 주소와 GPU 객체 |
| `FAssetManager` | Asset CPU 데이터와 Revision 관리, Resource Command 생성 | GPU Handle과 Draw 상태 |
| `FScene` | Primitive Command 병합, RT Proxy 소유와 적용 | Asset CPU 데이터와 GPU Resource 소유 |
| `FPrimitiveSceneProxy` | Render Thread 전용 Primitive 상태 | Component와 UObject 역참조 |
| `FSceneViewFamily` | Scene, View 배열, 출력 Target, Show Flag 묶음 | Scene과 Target 소유권 |
| `FSceneRenderer` | View별 컬링과 Pass Graph 구성 | World Tick, Present, Asset 로드 |
| `FRenderer` | 프레임 실행, Resource Cache, Shader·Pipeline·Sampler Cache | UObject와 Editor UI 정책 |
| `FRenderSystem` | Backend·Renderer·Render Thread 소유, GT/RT 제출 경계 | World와 Asset 로드 정책 |
| `FRenderThread` | FIFO 명령 실행 | Scene 또는 Pass 정책 |
| `IRenderDevice` | API 중립 GPU 자원·상태·Draw 계약 | World와 View 정책 |
| `IRenderContext` | Swap Chain, Back Buffer, Resize와 Present | Scene 순회 |
| `IRenderBackend` | Device·Context·Shader Format·ImGui Backend 묶음 | 렌더링 정책 |

`FRenderer`는 UObject가 아닌 일반 C++ 객체다. `FRenderSystem`이 `IRenderBackend`, `FRenderer`, `FRenderThread`를 프로세스 단위로 소유한다.

## 프레임 실행 순서

### Game/Editor Thread 제출

1. 입력과 UI를 처리하고 World를 Tick한다.
2. Component 변경 시 `FScene`에 Primitive Command를 제출한다.
3. Static Mesh와 Material 사용 시 `FAssetManager`에 필요한 Resource를 요청한다.
4. Viewport Client가 `FSceneViewFamily`와 출력 Target을 구성한다.
5. ImGui Draw Data를 `FImGuiDrawDataCopy`로 깊은 복사한다.
6. `FRenderSystem::Render()`가 Resource Command, Scene Command, ViewFamily와 UI 복사본을 하나의 Render Thread 작업으로 제출한다.

`FRenderSystem::Render()`는 최대 두 프레임까지 비동기로 제출한다. `SubmittedRenderFrames - CompletedRenderFrames`가 2에 도달했을 때만 Game Thread가 다음 제출을 기다린다.

### Render Thread 실행

Render Thread의 한 프레임 작업은 다음 순서를 고정한다.

```text
Texture Resource 갱신
    ↓
Static Mesh Resource 갱신
    ↓
Material Resource 갱신
    ↓
Scene Proxy Add / Update / Remove 적용
    ↓
BeginFrame
    ↓
ViewFamily별 SceneRenderer 실행
    ↓
ImGui 합성
    ↓
EndFrame → Submit → Present
    ↓
완료 프레임 번호와 통계 발행
```

Texture를 Material보다 먼저 갱신하므로 `FMaterialResource`가 Texture binding을 만들 때 대응하는 `FTextureResource`가 준비되어 있다. Scene Command는 Asset Resource 갱신 후 적용되므로 Proxy가 `AssetId`로 Mesh와 Material Resource를 안전하게 찾을 수 있다.

Native 출력의 Width 또는 Height가 0이면 GPU 프레임과 Present를 생략한다. Resource와 Scene Command 적용은 출력 크기 검사보다 먼저 수행한다.

## Scene Command와 Proxy

### Primitive 등록과 부분 갱신

```text
OnRegister
    → FPrimitiveRenderData(All) 생성
    → Proxy 생성
    → Add Command 제출
    → PrimitiveId 보관

Transform / Mesh / Material / Visibility 변경
    → 변경 항목의 값만 추출
    → Update Command 제출
    → 동일 PrimitiveId의 대기 명령과 병합

OnUnregister
    → Remove Command 제출
    → PrimitiveId 초기화
```

`ERenderCommandType`은 다음 비트 조합을 사용한다.

| 타입 | 전달하는 상태 |
|---|---|
| `Transform` | WorldMatrix |
| `Mesh` | WorldMatrix, LocalBounds, MeshAssetId, LOD 활성화 여부 |
| `Material` | Section Slot별 MaterialAssetId |
| `Visibility` | Visible, Selected |
| `All` | 위 상태 전체 |

`FScene::UpdatePrimitive()`는 같은 `PrimitiveId`의 마지막 대기 명령에 값을 합친다. Remove 전에 아직 적용하지 않은 Add가 있으면 Add를 취소하여 불필요한 Proxy 생성과 제거를 피한다.

### Proxy 저장소

Render Thread의 `FScene::ApplyRenderCommands()`가 Proxy를 밀집 `TArray`에 보관하고 `PrimitiveId → Array Index` Map으로 찾는다. 제거 시 마지막 Proxy를 빈 위치로 옮겨 배열을 밀집 상태로 유지한다.

`FPrimitiveSceneProxy`는 다음 공통 상태를 가진다.

- WorldMatrix
- LocalBounds와 WorldBounds
- WorldBoundsRadius
- Visible과 Selected

`FStaticMeshSceneProxy`는 여기에 `FStaticMeshResource*`, Section Material에 대응하는 `FMaterialResource*` 배열, LOD 활성화 상태를 추가한다. 모두 Renderer Cache가 소유하는 객체에 대한 비소유 참조이며 Component나 Asset UObject 포인터를 보관하지 않는다.

## Asset Resource Command

Asset Resource는 Primitive와 분리하여 Asset별 Revision 단위로 갱신한다.

| 명령 | 주요 데이터 | RT 결과 |
|---|---|---|
| `FStaticMeshResourceCommand` | AssetId, Revision, LOD Mesh 데이터 | `FStaticMeshResource`와 LOD별 Vertex/Index Buffer |
| `FTextureResourceCommand` | AssetId, Revision, Desc, 전체 Mip | `FTextureResource`와 Texture Handle |
| `FMaterialResourceCommand` | AssetId, Revision, Material 값, Parameter, Texture AssetId, Sampler | `FMaterialResource`와 Pipeline·binding |

`FAssetManager`는 요청한 `AssetId + Revision`을 기억한다. 같은 Revision은 다시 전송하지 않고, 최초 요청이나 Revision 변경만 Pending Resource Command에 추가한다. Primitive 수와 관계없이 Mesh Vertex/Index와 Texture Mip은 한 번만 전달된다.

`FRenderer`는 `FAssetId`를 키로 Resource를 `unique_ptr`로 소유한다. Map 재해시에도 Resource 주소가 유지되므로 Proxy와 Material binding의 비소유 포인터가 안정적이다.

### Material Resource

`FMaterialResource`는 다음 데이터를 준비한다.

- 일반 Static Mesh Pipeline
- 지원되는 Material의 Instanced Pipeline
- Shader Reflection으로 만든 Material Constant Layout과 값
- Texture Resource와 Sampler binding
- Opaque 정렬용 SortId
- 원본 Asset Revision

기본 Material과 1×1 White Texture는 `FRenderer`가 별도로 소유한다. Material이 없으면 Magenta Base Color와 White Texture를 사용한다.

Instanced Pipeline은 현재 기본 `StaticMesh.hlsl/MainVS`를 사용하는 비투명 Material에만 추가한다. Custom Vertex Shader 또는 Translucent Material은 일반 Pipeline으로 폴백한다.

## View, Target과 가시성

### FSceneView와 ViewFamily

`FSceneView`는 다음 View 단위 데이터를 보관한다.

- View, Projection, ViewProjection Matrix
- ViewOrigin과 FarClip
- Frustum
- 출력 `FRenderViewport`
- LOD 1~4 화면 비율 임계값

Knot Engine은 행벡터 규약을 사용한다. `ViewProjectionMatrix = ViewMatrix * ProjectionMatrix`이며 일반 Static Mesh Vertex Shader는 `Position * WorldMatrix * ViewProjectionMatrix` 순서로 변환한다.

`FSceneViewFamily`는 하나의 `FScene`, 여러 View, `FSceneRenderTarget`, `FShowFlags`를 묶는다. 현재 Show Flag는 Primitive, Axis, Grid, Bounds다.

### FSceneRenderTarget

| Target | 용도 |
|---|---|
| `SceneColor` | 선형 Scene Color, Post Process 입력 |
| `DisplayColor` | Post Process와 Overlay 결과, ImGui Viewport 표시 |
| `SelectionDepth` | 선택된 Primitive의 실루엣과 Reversed-Z Depth |
| `Depth` | 일반 Scene Depth |

Viewport Target 생성과 Resize는 Render Thread의 동기 명령으로 처리한다. ImGui Texture ID는 `DisplayColor` 생성 시 함께 얻어 Game Thread UI에서 재사용한다.

### Frustum Culling과 LOD

`FSceneRenderer::CullView()`는 View마다 Scene Proxy를 순회하고 다음 조건을 모두 만족한 Primitive만 수집한다.

- Visible 상태
- 유효한 WorldBounds
- Frustum 바깥이 아님

`FStaticMeshSceneProxy::SelectLOD()`는 투영 행렬, ViewOrigin, WorldBounds 중심과 반지름, View의 LOD 임계값으로 화면 비율을 근사한다. LOD가 비활성화되었거나 원근 투영이 아니면 LOD 0을 사용하고 현재 최대 5개 LOD를 선택한다.

## Opaque Draw Command와 자동 인스턴싱

### Primitive 배치 생성

`FOpaquePass::AddPass()`는 Visible Primitive마다 선택된 LOD, Mesh Buffer와 전체 Section Material 구성을 수집한다. LOD 주소와 Section Material Resource 주소를 조합한 Batch Key로 먼저 정렬하고, Hash가 같은 후보는 실제 LOD와 각 Section Material을 다시 비교한다.

같은 Instance Batch에 들어가려면 다음 조건을 모두 만족해야 한다.

- 같은 `FStaticMeshLODResource`
- 모든 Section에서 같은 `FMaterialResource`
- Index Buffer를 사용하는 Mesh
- 모든 Section Material에 유효한 Instanced Pipeline이 있음
- 같은 후보가 최소 4개 이상 존재함

조건을 만족하지 않으면 기존 Object Draw 경로를 사용한다. 자동 인스턴싱을 위해 Component나 별도 Instanced Mesh Asset을 요구하지 않는다.

### Instance Buffer

인스턴스마다 `FStaticMeshInstance` 하나가 64바이트 WorldMatrix를 저장한다.

```text
Input Slot 0
    FStaticMeshVertex
    PerVertex
    Position / Normal / Tangent / TexCoord

Input Slot 1
    FStaticMeshInstance
    PerInstance, StepRate 1
    INSTANCE_MODEL0..3
```

`FRenderer`는 재사용 가능한 Dynamic Instance Vertex Buffer 하나를 소유한다. 최초 필요 시 4096개 Instance 용량을 할당하고 부족하면 2배씩 확장한다. 각 Opaque Pass 실행 시 실제 사용한 Instance 범위만 업로드한다.

일반 Draw는 WorldMatrix를 `b3` Draw Constant로 보내고 `DrawIndexed()`를 호출한다. Instanced Draw는 Slot 0과 Slot 1을 함께 바인딩하고 `FirstInstance`, `InstanceCount`로 `DrawIndexedInstanced()`를 호출한다. 한 Primitive의 WorldMatrix는 Section 수와 무관하게 Instance 배열에 한 번만 저장되며 각 Section Draw가 같은 범위를 공유한다.

### Draw 정렬과 상태 캐시

현재 Opaque Sort Key는 다음과 같다.

```text
[ Pipeline 12bit | Material 20bit | Mesh 20bit | Reserved 12bit ]
```

Pipeline은 일반 또는 Instanced Pipeline Handle Index, Material은 Resource SortId, Mesh는 Vertex Buffer Handle Index를 사용한다. 현재 Depth는 Sort Key에 포함하지 않는다. 같은 Sort Key에서는 Section의 FirstIndex와 IndexCount로 순서를 안정화한다.

Pass 실행 중 직전에 바인딩한 Pipeline, Material, Mesh와 Instanced 여부를 추적한다. 값이 바뀔 때만 Pipeline, Material Constant·Texture·Sampler, Vertex/Index Buffer를 다시 설정한다.

### 인스턴싱 통계

Renderer는 프레임마다 다음 값을 누적하고 Render Thread 완료 Snapshot으로 발행한다.

- Visible Primitive 수
- Instanced Primitive 수
- Instance Batch 수
- Instanced Draw Call 수
- Fallback Draw Call 수
- Batch 구성 시간
- Instance Buffer 업로드 시간

Editor의 Profile Panel은 GPU/CPU 통계와 함께 마지막 완료 프레임의 `Instanced Draws` 통계를 표시한다.

## Render Pass와 Graph

### 현재 Pass

Pass는 장기 수명 객체나 공통 기반 클래스를 만들지 않는 정적 Node Builder다. `AddPass()`가 현재 View의 상수, Draw Command와 Handle을 캡처한 실행 함수를 `FRenderGraph`에 추가한다.

| Pass | 입력과 출력 |
|---|---|
| Opaque | SceneColor와 Depth에 Static Mesh 렌더링 |
| Selection | 선택된 Static Mesh를 SelectionDepth에 기록 |
| PostProcess | SceneColor와 SelectionDepth를 읽고 Outline과 Gamma Correction을 DisplayColor에 기록 |
| Overlay | Grid와 Axis를 DisplayColor에 합성 |
| DebugDraw | 동적 Line, Wire Shape와 Bounds를 DisplayColor에 합성 |

Family 시작 시 SceneColor, Depth와 SelectionDepth를 한 번 Clear한다. 각 Pass는 실행 직전에 자신의 Viewport와 필요한 Target을 바인딩한다. Family가 끝나면 Back Buffer를 복구하고, 모든 Family가 끝난 뒤 ImGui를 Back Buffer에 합성한다.

### 실행 순서

현재 SceneRenderer의 의존성은 다음과 같다.

```text
각 View:
    Opaque
        ↓
    Selection (선택된 Primitive가 있을 때)

모든 View의 Geometry 완료
        ↓
PostProcess
        ↓
Overlay / DebugDraw Nodes
```

여러 View는 현재 하나의 Graph 안에서 Geometry Node를 순차 연결한다. Overlay와 DebugDraw Node는 PostProcess 이후 수집된 순서로 연결한다.

`FRenderGraph`는 한 ViewFamily에서만 살아 있으며 raw `uint32` Node Index와 선행 Node 배열을 보관한다. 실행 시 의존성이 충족된 Node를 등록 순서대로 실행하고 순환 의존성을 검사한 뒤 폐기한다.

현재 Graph는 다음 기능을 제공하지 않는다.

- Texture read/write 선언
- Resource barrier와 state transition
- Transient Resource aliasing
- GPU Queue 분리
- Pass 병렬 실행

## Shader, Pipeline과 상수 계약

`FShaderRegistry`는 `FShaderKey`별 Shader를 최초 요청 시 생성한다. `FPipelineStateCache`는 완전한 `FPipelineStateDesc`별 Pipeline을 재사용하고 `FSamplerStateCache`는 Sampler Description별 Handle을 재사용한다.

Shader Stage별 Constant Buffer 슬롯은 다음 의미로 사용한다.

| 슬롯 | 의미 | 일반적인 갱신 시점 |
|---|---|---|
| `b0` | View constants | View마다 |
| `b1` | Pass constants | Pass마다 |
| `b2` | Material constants | Material 변경 시 |
| `b3` | Draw/Object constants | 일반 Draw마다 |

Static Mesh 일반 Vertex Shader는 `b0`과 `b3`을 사용한다. Instanced Vertex Shader는 Model Matrix를 Slot 1의 Per-Instance Vertex Input으로 받으므로 `b3`을 사용하지 않는다. Material Constant는 Shader Reflection이 찾은 Stage와 Slot에 바인딩한다.

## RHI와 D3D11 Backend

### 공통 계약

Engine은 구체 D3D11 타입을 참조하지 않는다. GPU 객체는 Index와 Generation으로 구성된 Handle로 참조하며 Handle 자체가 Resource 수명을 연장하지 않는다.

`IRenderDevice`는 다음 기능을 제공한다.

- Buffer, Texture, Shader, Pipeline과 Sampler 생성·파괴
- Command List Begin, End와 Submit
- Render Target, Viewport와 Pipeline 설정
- 단일 또는 복수 Vertex Buffer, Index Buffer 바인딩
- Constant, Texture와 Sampler 바인딩
- Draw, DrawIndexed와 DrawIndexedInstanced
- 비동기 GPU Frame Statistics

`FVertexElement`는 Semantic과 Format 외에 InputSlot, `PerVertex`/`PerInstance`, InstanceStepRate를 가진다. D3D11 Backend는 이를 `D3D11_INPUT_ELEMENT_DESC`의 InputSlotClass와 InstanceDataStepRate로 변환한다.

### D3D11 실행 모델

현재 `FCommandListHandle`은 RHI 실행 구간을 검증하는 논리 Handle이다. D3D11 Deferred Context나 Native Command List를 병렬 기록하는 구조가 아니며 실제 호출은 Render Thread의 Immediate Context에서 직렬 실행한다. 별도 RHI Thread는 없다.

향후 D3D12 Backend에는 Command allocator/list 재사용, Descriptor 관리, Resource state transition, Upload 수명과 GPU Fence 기반 지연 해제가 추가로 필요하다.

## 스레딩과 수명

### FRenderThread

`FRenderThread`는 `std::function<void()>` FIFO와 전용 `std::thread`를 가진다.

- `Enqueue()`는 비동기 제출한다.
- `EnqueueAndWait()`는 해당 명령 완료까지 호출 스레드를 대기시킨다.
- `Flush()`는 빈 동기 명령을 FIFO 뒤에 넣어 이전 작업 완료를 보장한다.
- `Shutdown()`은 Flush 후 Stop을 요청하고 Thread를 Join한다.

정상 프레임은 `Enqueue()`를 사용한다. Startup/Shutdown, Window Resize, Viewport Target Resize, UI Backend 초기화·종료, 즉시 결과가 필요한 Texture 생성과 ImGui Texture ID 조회는 `EnqueueAndWait()`를 사용한다.

### World와 Asset 종료

World 파괴 전에는 Component가 Remove Command를 제출한 뒤 `FRenderSystem::Flush(FScene&)`가 대기 명령을 적용한다. 따라서 `FScene`이 파괴될 때 Proxy와 Pending Command가 남지 않는다.

Asset 종료 전에는 `ReleaseAssetResources()`가 Render Thread에서 Renderer Resource Cache를 비우고 Asset Manager의 요청 Revision 기록을 초기화한다. Renderer 종료 시 Proxy와 Viewport/UI Resource가 먼저 정리되어 있어야 한다.

CPU 명령 실행 완료와 GPU 실행 완료는 같은 의미가 아니다. 현재 D3D11 수명 규칙을 사용하지만 D3D12에서는 GPU Fence 완료 후 Resource를 회수해야 한다.

### Snapshot과 통계

ViewFamily, Scene Command, Resource Command와 `FImGuiDrawDataCopy`는 제출된 Render Thread 작업이 소유한다. ImGui DX11 Backend는 Game Thread의 ImGui Context를 읽지 않고 복사된 Vertex, Index와 Draw Command만 사용한다.

CPU Profiler는 Game Thread와 Render Thread를 별도로 수집한다. GPU Query 결과와 Instanced Draw Statistics는 Render Thread가 완료 Snapshot으로 발행하며 Editor는 Mutex로 보호된 마지막 값을 읽는다.

## 현재 구현 상태

### 구현됨

- 전용 Render Thread와 최대 2 Frames In Flight
- PrimitiveId 기반 Scene Command와 동일 Primitive 갱신 병합
- Component와 Proxy의 상호 참조 제거
- AssetId + Revision 기반 Static Mesh, Texture, Material Resource Command
- Render Thread 전용 Proxy 및 GPU Resource Cache
- ViewFamily, 다중 View와 Frustum Culling
- 화면 비율 기반 Static Mesh LOD 선택
- SceneColor, DisplayColor, SelectionDepth와 Depth Target
- Opaque, Selection, PostProcess, Overlay와 DebugDraw Pass
- raw Node Index 의존성 기반 Frame-local Render Graph
- Shader, Pipeline과 Sampler Cache
- 기본 Shader 비투명 Static Mesh의 자동 인스턴싱
- Per-Instance Vertex Input과 다중 Vertex Buffer RHI 계약
- Pipeline → Material → Mesh Opaque 정렬과 중복 상태 바인딩 생략
- ImGui Draw Data 깊은 복사와 Render Thread 합성
- Thread별 CPU, 비동기 GPU와 Instanced Draw 통계

### 아직 분리하거나 구현하지 않은 것

- Translucency 전용 Pass와 Back-to-Front 정렬
- Shadow와 Light Pass
- Material 변경 시 사용 Primitive를 자동 재제출하는 무효화 전파
- Custom Vertex Shader용 Instanced Variant 생성 정책
- Skinned Mesh와 Animation 렌더 경로
- Render Graph Resource read/write와 자동 상태 전이
- RHI Thread
- D3D12 Backend와 GPU Fence 기반 지연 해제

현재 Translucent Material도 별도 Translucency Pass 없이 Opaque Pass에서 일반 Pipeline으로 제출된다. 자동 인스턴싱에서는 제외되지만 정확한 투명 정렬은 아직 제공하지 않는다.

## 검증 기준

- 같은 PrimitiveId의 여러 부분 갱신이 다음 제출 전에 하나의 Command로 병합된다.
- Resource Command가 Texture → Mesh → Material 순서로 적용되고 Scene Command보다 먼저 완료된다.
- Render Thread가 Asset UObject나 Component를 조회하지 않는다.
- Mesh 재임포트 후 AssetId는 유지되고 Revision 변경으로 GPU LOD Resource가 갱신된다.
- 부모 Transform과 Inspector 편집이 다음 Render 제출의 Proxy에 반영된다.
- World 종료 시 Remove Command 적용 후 Proxy와 Pending Command가 남지 않는다.
- 여러 View가 독립적으로 컬링하고 Family Target은 한 번만 Clear된다.
- 선택된 Primitive만 SelectionDepth에 기록되고 PostProcess Outline에 반영된다.
- 같은 LOD와 Material 구성의 Primitive가 4개 이상이면 Instanced Draw로 병합된다.
- 지원하지 않는 Material과 작은 Batch는 일반 Draw로 폴백한다.
- 인스턴싱 전후 출력 Transform과 Section Material이 동일하다.
- Instanced Draw 통계의 Primitive, Batch와 Draw 수가 실제 제출과 일치한다.
- ImGui 합성은 Game Thread Context를 읽지 않고 복사본만 사용한다.

이 목록은 구조적 검증 계약이며 모든 항목이 자동 테스트로 구현되어 있다는 뜻은 아니다.

## 관련 파일

- [AssetManager.cpp](../KnotEngine/Source/Engine/Asset/AssetManager.cpp)
- [PrimitiveComponent.cpp](../KnotEngine/Source/Engine/Component/PrimitiveComponent.cpp)
- [StaticMeshComponent.cpp](../KnotEngine/Source/Engine/Component/Mesh/StaticMeshComponent.cpp)
- [PrimitiveSceneProxy.h](../KnotEngine/Source/Engine/Render/Proxy/PrimitiveSceneProxy.h)
- [Scene.h](../KnotEngine/Source/Engine/Render/Scene/Scene.h)
- [Scene.cpp](../KnotEngine/Source/Engine/Render/Scene/Scene.cpp)
- [SceneView.h](../KnotEngine/Source/Engine/Render/Scene/SceneView.h)
- [SceneRenderer.cpp](../KnotEngine/Source/Engine/Render/Scene/SceneRenderer.cpp)
- [ResourceCommand.h](../KnotEngine/Source/Engine/Render/Resource/ResourceCommand.h)
- [MaterialResource.cpp](../KnotEngine/Source/Engine/Render/Resource/MaterialResource.cpp)
- [StaticMeshResource.cpp](../KnotEngine/Source/Engine/Render/Resource/Mesh/StaticMeshResource.cpp)
- [Vertex.h](../KnotEngine/Source/Engine/Render/Resource/Mesh/Vertex.h)
- [OpaquePass.cpp](../KnotEngine/Source/Engine/Render/Pass/OpaquePass.cpp)
- [SelectionPass.cpp](../KnotEngine/Source/Engine/Render/Pass/SelectionPass.cpp)
- [PostProcessPass.cpp](../KnotEngine/Source/Engine/Render/Pass/PostProcessPass.cpp)
- [OverlayPass.cpp](../KnotEngine/Source/Engine/Render/Pass/OverlayPass.cpp)
- [DebugDrawPass.cpp](../KnotEngine/Source/Engine/Render/Pass/DebugDrawPass.cpp)
- [RenderGraph.cpp](../KnotEngine/Source/Engine/Render/Graph/RenderGraph.cpp)
- [Renderer.cpp](../KnotEngine/Source/Engine/Render/Renderer.cpp)
- [RenderDevice.h](../KnotEngine/Source/Engine/Render/RHI/RenderDevice.h)
- [VertexLayout.h](../KnotEngine/Source/Engine/Render/RHI/VertexLayout.h)
- [RenderSystem.cpp](../KnotEngine/Source/Renderer/Render/RenderSystem.cpp)
- [RenderThread.cpp](../KnotEngine/Source/Renderer/Render/RenderThread.cpp)
- [D3D11RenderDevice.cpp](../KnotEngine/Source/Renderer/Render/D3D11/D3D11RenderDevice.cpp)
- [ImGuiDrawDataCopy.cpp](../KnotEngine/Source/Renderer/Render/ImGui/ImGuiDrawDataCopy.cpp)
- [StaticMesh.hlsl](../KnotEngine/Content/Engine/Shader/StaticMesh.hlsl)
- [Input-Architecture.md](Input-Architecture.md)
- [Conventions.md](Conventions.md)
