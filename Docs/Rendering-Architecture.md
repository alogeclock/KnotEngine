# Knot Engine Rendering Architecture

## 문서 목적

이 문서는 Knot Engine의 장면 데이터 수집, View별 가시성 판정, Render Pass 구성, Draw Command 정렬, RHI 명령 기록과 화면 출력의 책임을 정의한다.

현재 Knot Engine은 Direct3D 11 Immediate Context를 사용하는 단순한 Forward 렌더러다. 장기적으로 Direct3D 12 백엔드로 교체하고 Shadow, Light, Translucency 등의 Pass를 추가하더라도 상위 렌더링 구조를 다시 작성하지 않는 것을 목표로 한다.

초기 구현에서는 고정된 Pass 순서를 사용한다. Render Graph, 범용 Pass 기반 클래스, 비동기 Render Thread는 실제 필요가 생겼을 때 도입한다..

## 설계 원칙

- `FScene`은 View에 독립적인 Primitive와 Light proxy 포인터를 non-owning으로 등록한다.
- `FSceneRenderer`는 한 프레임에 필요한 View를 만들고 View별 가시성을 계산하며 Pass 실행 순서를 조율한다.
- 가시성 판정과 Draw Command 생성은 서로 다른 단계로 구분한다.
- 각 Render Pass는 자신의 View와 가시성 결과를 사용해 Draw Command를 생성하고 정렬한다.
- 동일한 Primitive라도 Pass와 View가 다르면 별도의 Draw Command를 만들 수 있다.
- `IRenderDevice`는 Scene, Light, Material 정책을 알지 않고 이미 정렬된 GPU 명령만 기록한다.
- `IRenderContext`는 Native Window, Swap Chain, Back Buffer와 Present를 관리한다.
- Render Target 바인딩과 Clear는 `IRenderContext`가 아니라 명령을 기록하는 rendering scope가 담당한다.
- Direct3D 11과 Direct3D 12의 차이는 RHI 백엔드 안에 가둔다.
- 네이티브 D3D 객체는 백엔드 밖으로 노출하지 않고 generation이 포함된 Render Handle로 참조한다.
- 기본 렌더링 경로는 단순한 Forward 렌더링으로 유지한다.
- 에디터 ImGui 렌더링은 엔진 장면 Pass가 끝난 뒤 마지막 Overlay Pass에서 수행한다.

## 전체 구조

현재 구현은 Component가 `URenderer`에 직접 상수 데이터와 Mesh Draw를 제출한다.

```text
UPrimitiveComponent
        ↓
URenderer::UpdateConstant / DrawMeshBuffer
        ↓
IRenderDevice
        ↓
FD3D11RenderDevice
        ↓
ID3D11DeviceContext
```

이 경로는 하나의 불투명 Mesh를 그리기에는 충분하지만 다음 기능을 지원하기 어렵다.

- 반투명 Primitive의 View depth 정렬
- 카메라 밖에 있는 Shadow caster 수집
- 동일 Primitive의 Shadow와 Main Pass 중복 참여
- Pass별 Shader와 Pipeline 선택
- Offscreen Render Target과 여러 View 관리

장기 목표 구조는 다음과 같다.

```text
UPrimitiveComponent / LightComponent
        ↓ Add, Update, Remove
FScene
├─ FPrimitiveSceneProxy
└─ FLightSceneProxy
        ↓
FSceneRenderer
├─ Main View 준비와 가시 Primitive 수집
├─ Shadow View 준비와 View별 가시 Primitive 수집
└─ Render Pass 실행 순서 결정
        ↓
Pass별 Draw Command 생성과 정렬
├─ Shadow Pass
├─ Opaque/Base Pass
├─ Translucency Pass
└─ Overlay Pass
        ↓
IRenderDevice
├─ Begin
├─ Pipeline과 Resource 설정
├─ Draw
└─ End
        ↓
IRenderContext::Present
```

## 디렉터리와 책임

```text
KnotEngine/Source/
├─ Engine/
│  ├─ Components/
│  │  ├─ PrimitiveComponent.h/.cpp
│  │  └─ LightComponent.h/.cpp               향후 구현
│  └─ Render/
│     ├─ Renderer.h/.cpp
│     ├─ Scene.h/.cpp
│     ├─ SceneRenderer.h/.cpp                향후 구현
│     ├─ RenderPass.h/.cpp                   필요할 때 분리
│     ├─ Resource/
│     ├─ RHI/
│     │  ├─ RenderTypes.h
│     │  ├─ RenderDevice.h
│     │  └─ RenderContext.h
│     ├─ D3D11/
│     └─ D3D12/                              향후 구현
└─ Editor/
   └─ UI/ImGui/
```

초기에는 Pass마다 파일과 클래스를 만들지 않는다. `FSceneRenderer`의 `RenderShadowPass()`, `RenderOpaquePass()`, `RenderTranslucencyPass()` 같은 함수로 시작하고 구현이 커질 때만 `FRenderPass`로 분리한다.

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `UPrimitiveComponent` | Mesh, Material, Transform 변경을 Scene에 전달 | View culling, Draw 호출, Pipeline 선택 |
| `FScene` | Primitive와 Light proxy의 non-owning 등록과 조회 | Proxy 소유권, View별 가시 Primitive, Pass 정렬 |
| `FSceneRenderer` | View 준비, 가시성 판정, Pass 스케줄링 | 네이티브 D3D 호출 |
| `FRenderPass` | Pass 참여 필터링, Draw Command 생성, Sort Key 계산과 제출 | Scene 객체 수명 관리 |
| `URenderer` | 렌더 시스템 수명, Frame 시작과 종료, SceneRenderer 실행 | Component별 렌더 정책 |
| `IRenderDevice` | GPU 자원, Pipeline, Command List와 Draw 명령 | Swap Chain, Scene 정책 |
| `IRenderContext` | Native Window, Swap Chain, Back Buffer, Resize와 Present | 일반 Texture, Shader, Pass 정렬 |
| D3D11/D3D12 백엔드 | RHI 계약을 네이티브 API로 변환 | 엔진 장면과 Material 정책 |
| ImGui Render Backend | Overlay Pass에서 ImGui draw data 기록 | 장면 Pass 실행 순서 결정 |

## 프레임 실행 순서

### 현재 구현

현재 `UEditorEngine`의 렌더링은 다음 순서로 실행된다.

```text
URenderer::BeginFrame
    ├─ BeginCommandList
    ├─ Back Buffer와 Depth Target Clear
    └─ 기본 Graphics Pipeline 설정
        ↓
UPrimitiveComponent::Render
    ├─ World/View/Projection 상수 설정
    └─ Mesh Draw
        ↓
ImGui Draw
        ↓
URenderer::EndFrame
    ├─ EndCommandList
    ├─ Submit
    └─ Present
```

D3D11 백엔드에서는 논리 Command List가 열린 구간을 검증하며 실제 명령은 Immediate Context에 즉시 실행된다.

### 목표 실행 순서

SceneRenderer가 도입된 뒤에는 다음 순서를 따른다.

1. `IRenderContext`에서 현재 Back Buffer를 획득한다.
2. `FSceneRenderer`가 Main View를 준비한다.
3. Main View에 영향을 주는 Light를 선정한다.
4. Shadow를 생성할 Light에서 Shadow View를 만든다.
5. Main View와 각 Shadow View에 대해 가시성을 계산한다.
6. Shadow Pass를 Shadow View별로 실행한다.
7. Main View의 Opaque/Base Pass를 실행한다.
8. 렌더링 방식에 필요하면 Light Pass를 실행한다.
9. Main View의 Translucency Pass를 실행한다.
10. 에디터 UI를 Overlay Pass에서 그린다.
11. Command List를 종료하고 제출한 뒤 Present한다.

```text
Acquire Back Buffer
        ↓
Prepare Main View
        ↓
Select Relevant Lights
        ↓
Build Shadow Views
        ↓
Cull Main View + Cull Each Shadow View
        ↓
Shadow Pass 0..N
        ↓
Opaque/Base Pass
        ↓
Optional Light Pass
        ↓
Translucency Pass
        ↓
Overlay Pass
        ↓
Submit and Present
```

## Scene 렌더 데이터

### FScene

`FScene`은 게임 객체 자체가 아니라 렌더링에 필요한 proxy 포인터를 등록한다. Proxy는 Component가 소유하고 `FScene`은 non-owning 포인터만 보관한다.

```cpp
class FScene
{
public:
	void AddPrimitive(FPrimitiveSceneProxy* Proxy);
	void UpdatePrimitive(FPrimitiveSceneProxy* Proxy);
	void RemovePrimitive(FPrimitiveSceneProxy* Proxy);

	void AddLight(FLightSceneProxy* Proxy);
	void UpdateLight(FLightSceneProxy* Proxy);
	void RemoveLight(FLightSceneProxy* Proxy);

private:
	TArray<FPrimitiveSceneProxy*> Primitives;
	TArray<FLightSceneProxy*> Lights;
};
```

Proxy 포인터에는 다음 수명 규칙을 적용한다.

- Component가 Proxy를 생성하고 소유한다.
- 등록된 Proxy의 주소는 Scene에서 제거될 때까지 바뀌지 않는다.
- Component는 Proxy를 파괴하기 전에 반드시 `FScene`에서 제거한다.
- `UpdatePrimitive()`와 `UpdateLight()`는 변경된 Transform과 Bounds를 Scene의 공간 구조와 동기화하는 진입점이다.
- 현재 단일 스레드 구현에서는 제거 직후 Proxy를 파괴할 수 있다.
- Render Thread를 도입하면 제거 명령이 처리될 때까지 Proxy 파괴를 지연한다.

Scene 요소를 위한 별도 Handle은 처음부터 만들지 않는다. Packed array 재배치, 재등록 전후의 안정적인 식별, 비동기 명령의 stale reference 검출 또는 GPU Scene index가 필요해질 때 포인터와 별도의 ID를 추가한다. GPU 자원을 나타내는 `FBufferHandle`, `FTextureHandle` 등의 Render Handle은 이 Scene 참조 정책과 무관하게 유지한다.

Scene proxy에는 다음과 같은 View 독립 데이터가 들어간다.

| 데이터 | 용도 |
|---|---|
| Mesh와 Material 참조 | Pass별 Draw Command 생성 |
| Local-to-World Transform | View별 상수와 Bounds 변환 |
| World Bounds | Frustum culling |
| Blend Mode | Opaque, Masked, Translucent 분류 |
| Cast/Receive Shadow 플래그 | Shadow Pass와 Base Pass 필터링 |
| Light 종류와 영향 범위 | Main View 영향 판정과 Shadow View 생성 |

`FScene`은 하나의 전역 Draw Command 배열을 만들거나 정렬하지 않는다. 최종 명령은 View와 Pass에 따라 달라지기 때문이다.

### View와 가시 Primitive

`FViewInfo`는 Scene을 어느 위치와 투영 방식으로 바라보는지를 나타내는 프레임 단위 데이터다. Main Camera, Directional Shadow cascade, Spot Light Shadow와 Scene Capture는 모두 같은 기본 구조를 사용한다.

```cpp
struct FViewInfo
{
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
	FMatrix ViewProjectionMatrix;
	FVector ViewOrigin = FVector::ZeroVector;
	FFrustum Frustum;
	FRenderViewport Viewport;
};
```

| 데이터 | 의미 |
|---|---|
| `ViewMatrix` | World 공간을 View 공간으로 변환한다. |
| `ProjectionMatrix` | View 공간을 Clip 공간으로 변환하며 Perspective와 Orthographic 투영을 모두 표현한다. |
| `ViewProjectionMatrix` | `ViewMatrix * ProjectionMatrix`를 미리 계산한 값으로 culling과 shader 상수 생성에 사용한다. |
| `ViewOrigin` | World 공간의 관찰 위치로 거리 기반 LOD, Light 판정과 정렬에 사용한다. |
| `Frustum` | `ViewProjectionMatrix`에서 만든 절두체로 Primitive Bounds를 판정한다. |
| `Viewport` | Render Target 안에서 rasterization할 사각 영역과 depth 범위를 나타낸다. |

View를 만들 때 행렬과 파생 데이터를 한 번에 완성하고, 해당 View의 culling과 Pass 실행이 끝날 때까지 변경하지 않는다.

```cpp
FViewInfo View;
View.ViewMatrix = ViewMatrix;
View.ProjectionMatrix = ProjectionMatrix;
View.ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;
View.ViewOrigin = ViewOrigin;
View.Frustum.UpdateFromCamera(View.ViewProjectionMatrix);
View.Viewport = Viewport;
```

별도의 View 종류 enum은 두지 않는다. Perspective와 Orthographic의 차이는 `ProjectionMatrix`에 들어 있고, Main View인지 Shadow View인지는 `FSceneRenderer`가 View를 전달하는 Pass 문맥으로 구분할 수 있다.

`FViewInfo`는 관찰 조건만 표현하며 다음 데이터는 소유하지 않는다.

- 가시 Primitive 배열
- Render Target과 Shadow Map
- Opaque, Shadow, Translucency Draw Command
- Camera Component와 Light Component 같은 게임 객체
- 이전 프레임의 temporal history

Main View의 가시 Primitive 배열은 `FSceneRenderer`가 보관한다. Shadow Map과 Light 참조처럼 Shadow Pass에만 필요한 값은 `FShadowViewInfo`가 View와 함께 보관한다.

가시 Primitive 목록은 Scene 전체에 하나가 아니라 View마다 존재한다. Main View의 결과는 `FSceneRenderer::MainVisiblePrimitives`에 보관하고, Shadow View는 자신의 배열을 직접 가진다.

```cpp
struct FShadowViewInfo
{
	FViewInfo View;
	TArray<const FPrimitiveSceneProxy*> VisiblePrimitives;
	const FLightSceneProxy* Light = nullptr;
	FTextureViewHandle ShadowTarget;
};
```

Main View의 배열은 Opaque와 Translucency Pass가 공유할 수 있다. Shadow Pass는 Main View 밖의 Primitive도 필요하므로 각 Shadow View에서 별도의 배열을 만든다.

```text
MainVisiblePrimitives
├─ Opaque/Base Pass
└─ Translucency Pass

Directional Light
├─ Cascade 0 VisiblePrimitives → Shadow Pass
├─ Cascade 1 VisiblePrimitives → Shadow Pass
└─ Cascade N VisiblePrimitives → Shadow Pass
```

Light 선정은 Main View에 보이는 Primitive 목록을 기준으로 하지 않는다. Light의 영향 범위가 Main View에 영향을 주는지 별도로 판단한 뒤 Shadow View를 만든다.

### 가시 Primitive 수집과 Draw Command 수집

문서와 코드에서 두 종류의 수집을 구분한다.

| 단계 | 입력 | 출력 | 소유자 |
|---|---|---|---|
| 가시 Primitive 수집 | `FScene`, `FViewInfo` | `TArray<const FPrimitiveSceneProxy*>` | `FSceneRenderer` |
| Draw Command 수집 | `FViewInfo`, 가시 Primitive 배열, Pass 조건 | `FMeshDrawCommand` | 각 Render Pass |

Scene의 공간 구조를 탐색하는 작업은 View마다 한 번 수행한다. 같은 View를 사용하는 Pass들은 가시 Primitive 배열을 공유하고, 그 안에서 자신의 조건에 맞는 Primitive를 다시 필터링한다.

## Draw Command

`FMeshDrawCommand`는 한 Pass에서 하나의 Mesh draw를 실행하는 데 필요한 데이터와 Sort Key를 보관한다.

```cpp
struct FMeshDrawCommand
{
	FGraphicsPipelineHandle Pipeline;
	FBufferHandle VertexBuffer;
	FBufferHandle IndexBuffer;
	uint32 IndexCount = 0;
	uint32 FirstIndex = 0;
	int32 VertexOffset = 0;
	uint64 SortKey = 0;
};
```

동일 Primitive라도 Pass마다 Shader, Pipeline, Resource binding과 상수가 다르므로 별도의 Draw Command를 만들 수 있다.

```text
Primitive A
├─ Shadow View 0 Command
│  └─ Depth-only Pipeline
├─ Shadow View 1 Command
│  └─ Depth-only Pipeline
├─ Opaque Command
│  └─ Material Pipeline
└─ Optional Depth Command
   └─ Depth-only Pipeline
```

초기에는 Draw Command 배열을 Pass 함수의 지역 데이터로 만들고 정렬한 뒤 즉시 제출한다. 정적 Mesh 캐시나 병렬 Command 생성은 실제 비용을 측정한 뒤 추가한다.

## Render Pass 관리

### 고정 Pass 스케줄

초기 SceneRenderer는 Pass 객체 목록이나 의존성 그래프 대신 명시적인 함수 호출 순서를 사용한다.

```cpp
void FSceneRenderer::Render(const FScene& Scene, const FViewInfo& MainView)
{
	CullScene(Scene, MainView, MainVisiblePrimitives);
	PrepareShadowViews(Scene, MainView);

	for (FShadowViewInfo& ShadowView : ShadowViews)
	{
		CullScene(Scene, ShadowView.View, ShadowView.VisiblePrimitives);
		RenderShadowPass(ShadowView);
	}

	RenderOpaquePass(MainView, MainVisiblePrimitives);
	RenderTranslucencyPass(MainView, MainVisiblePrimitives);
	RenderOverlayPass();
}
```

실제로 필요하지 않은 Pass 함수는 호출하지 않는다. 기본 Forward 경로에는 별도 Light Pass를 두지 않으며, Deferred 경로가 추가되면 Opaque/GBuffer와 Translucency 사이에 `RenderLightPass()`를 삽입한다.

Pass 구현이 커지거나 여러 렌더링 경로에서 재사용될 때 다음 단위로 분리할 수 있다.

```text
FSceneRenderer
        ↓ Pass 실행 조율
FMeshPassProcessor
        ↓ Mesh와 Material을 Pass 전용 Command로 변환
FMeshDrawCommand 배열
        ↓ 정렬과 제출
IRenderDevice
```

`FRenderPass` 가상 기반 클래스는 공통 계약이 실제로 확인되기 전까지 만들지 않는다.

### Shadow Pass

Shadow Pass는 Shadow View마다 독립적으로 실행한다.

- `CastShadow`가 설정된 Primitive만 포함한다.
- Shadow View frustum으로 가시 Primitive를 수집한다.
- 일반적으로 Translucent Primitive는 제외한다.
- Masked Material은 alpha test가 가능한 Shadow shader permutation을 사용한다.
- Color attachment와 Pixel Shader가 없는 depth-only Pipeline을 허용한다.
- Directional Light cascade, Spot Light, Point Light face는 각각 별도 Shadow View로 표현한다.

Shadow Map Texture는 Depth Stencil과 Shader Resource 용도를 함께 가져야 한다. Shadow Pass가 끝난 뒤 Base 또는 Light Pass에서 sampling한다.

### Opaque/Base Pass

Opaque/Base Pass는 `MainVisiblePrimitives`에서 Opaque와 Masked Primitive를 선택한다.

- Depth test와 depth write를 활성화한다.
- Opaque는 blending을 비활성화한다.
- Masked는 shader에서 alpha test를 수행한다.
- Pipeline, Material, Mesh 기준으로 상태 변경을 줄이도록 정렬한다.
- 필요한 경우 View depth를 Sort Key의 하위 기준으로 사용한다.

기본 Forward 경로에서는 이 Pass가 Material과 Light를 함께 평가한다.

### Light Pass

Forward 렌더링에서는 별도의 Light Pass가 필수가 아니다. `FSceneRenderer`가 Main View에 영향을 주는 Light를 선정하고 Opaque/Base Pass에 Light 데이터를 전달하면 된다.

향후 Deferred 렌더링을 도입하면 다음 순서의 별도 Light Pass를 추가할 수 있다.

```text
GBuffer Pass
        ↓
Light Pass
        ↓
Translucency Pass
```

이 경우 Light Pass는 GBuffer와 Shadow Map을 읽어 Scene Color에 조명을 합성한다. View 준비, 가시 Primitive 수집, 고정 Pass 스케줄과 RHI rendering scope는 그대로 재사용한다.

따라서 `LightPass`라는 이름을 미리 빈 클래스로 만들지 않고, 실제 렌더링 경로가 결정될 때 `RenderLightPass()` 또는 별도 구현을 추가한다.

### Translucency Pass

Translucency Pass는 `MainVisiblePrimitives`에서 Translucent Material만 선택한다.

- 일반적으로 depth test는 활성화하고 depth write는 비활성화한다.
- Source Alpha와 Inverse Source Alpha blending을 사용한다.
- Primitive의 View depth를 기준으로 원거리에서 근거리 순서로 정렬한다.
- Opaque Pass가 완성한 Scene Color와 Depth를 사용한다.

Pipeline 상태만 바꾸어 즉시 그리는 방식으로는 올바른 정렬을 보장할 수 없으므로, Translucent Draw Command는 반드시 먼저 수집한 뒤 정렬한다.

초기에는 Primitive 단위 정렬만 지원한다. Mesh triangle 단위 정렬, Order Independent Transparency, 별도 Translucency Layer는 필요가 확인될 때 추가한다.

### Optional Depth Pass

Depth Prepass는 초기 Pass 목록에 포함하지 않는다. Opaque overdraw, occlusion culling, 복잡한 lighting 비용으로 이득이 확인될 때 `MainVisiblePrimitives`를 재사용해 추가한다.

### Overlay Pass

Overlay Pass는 장면 렌더링이 끝난 Back Buffer에 에디터 UI를 그린다.

- ImGui draw data의 제출 순서를 보존한다.
- 엔진 장면의 Opaque 또는 Translucency 정렬에 참여하지 않는다.
- 네이티브 ImGui 백엔드가 D3D 상태를 변경하므로 마지막 Pass로 제한한다.
- 장기적으로 D3D12 ImGui 백엔드도 현재 Command List에 명령을 기록하도록 한다.

## Pass별 정렬

정렬 기준은 Scene 전체가 아니라 Pass가 결정한다.

| Pass | 기본 정렬 기준 |
|---|---|
| Shadow | Pipeline, Material permutation, Mesh |
| Optional Depth | Pipeline, Mesh |
| Opaque/Base | Pipeline, Material, Mesh, View depth |
| Light | Light 종류와 Pipeline 또는 screen tile |
| Translucency | View depth 원거리에서 근거리 |
| Overlay | Layer와 제출 순서 |

Sort Key는 Pass가 Command를 만들 때 계산한다. `IRenderDevice`는 Sort Key의 의미를 알지 않고 전달된 순서대로 명령을 기록한다.

## RHI와 백엔드 추상화

### 공통 계약

RHI는 엔진의 렌더링 정책이 아니라 GPU 작업에 필요한 최소 기능을 제공한다.

```text
FSceneRenderer / Render Pass
        ↓ API 중립 명령
IRenderDevice + IRenderContext
        ↓
FD3D11RenderDevice + FD3D11RenderContext
또는
FD3D12RenderDevice + FD3D12RenderContext
```

공통 Handle은 Index와 Generation으로 구성한다.

```text
FBufferHandle
FTextureHandle
FTextureViewHandle
FShaderHandle
FGraphicsPipelineHandle
FCommandListHandle
```

백엔드는 Handle을 네이티브 자원으로 해석하고 유효하지 않은 Index, Generation, 사용 상태를 검증한다.

### IRenderDevice

`IRenderDevice`는 다음 기능을 담당한다.

- Buffer, Texture, Texture View, Shader, Pipeline 생성과 파괴
- Command List 시작, 종료와 제출
- Rendering scope 시작과 종료
- Pipeline, Vertex/Index Buffer, Shader Resource와 상수 설정
- Draw와 Dispatch 명령 기록

Rendering scope는 Render Target binding과 Clear를 명시적으로 표현한다.

```cpp
struct FRenderingDesc
{
	std::span<const FRenderingAttachment> ColorAttachments;
	const FDepthStencilAttachment* DepthStencilAttachment = nullptr;
	FRenderViewport Viewport;
};

virtual void BeginRender(FCommandListHandle CommandList, const FRenderingDesc& Desc) = 0;
virtual void EndRender(FCommandListHandle CommandList) = 0;
```

Shadow Pass는 Depth attachment만 사용하고 Main Pass는 Back Buffer와 Main Depth attachment를 사용한다.

### IRenderContext

`IRenderContext`는 창과 Swap Chain 수명에 종속된 기능만 담당한다.

```cpp
virtual void Create(void* NativeWindowHandle) = 0;
virtual void Release() = 0;
virtual void Resize(uint32 Width, uint32 Height) = 0;
virtual FTextureViewHandle AcquireBackBuffer() = 0;
virtual void Present() = 0;
virtual FRenderViewport GetViewport() const = 0;
```

현재 `BeginFrame()`에 들어 있는 Back Buffer Clear와 Render Target binding은 장기적으로 `IRenderDevice::BeginRender()`로 이동한다. 그래야 같은 Command List에서 Shadow Map과 Back Buffer를 순서대로 열 수 있다.

### D3D11 백엔드

D3D11 구현은 Immediate Context를 사용하지만 상위 계층에는 논리 Command List로 보인다.

| RHI 동작 | D3D11 구현 |
|---|---|
| `BeginCommandList` | 열린 기록 구간과 Generation 검증 시작 |
| `BeginRender` | Clear, `OMSetRenderTargets`, `RSSetViewports` |
| Pipeline 설정 | Input Layout, Shader, Blend, Rasterizer, Depth State 설정 |
| Resource 설정 | `VSSet*`, `PSSet*`, `IASet*` 호출 |
| Draw | Immediate Context에서 즉시 실행 |
| `EndCommandList` | 기록 구간 종료 검증 |
| `Submit` | Handle 소비와 상태 검증 |

State cache와 동적 Constant Buffer 재사용은 D3D11 백엔드 내부 최적화다. 상위 Renderer는 해당 구현을 알지 않는다.

### D3D12 백엔드

D3D12 구현은 동일한 RHI 계약을 실제 Command Allocator와 Graphics Command List로 변환한다.

| RHI 동작 | D3D12 구현 |
|---|---|
| `BeginCommandList` | Frame allocator reset과 Command List open |
| `BeginRender` | Resource barrier, RTV/DSV binding과 Clear |
| Pipeline 설정 | PSO와 Root Signature 설정 |
| Resource 설정 | Descriptor table, root CBV와 upload allocation |
| Draw | Graphics Command List에 기록 |
| `EndCommandList` | Command List close |
| `Submit` | Command Queue 실행과 Fence 기록 |

D3D12의 Descriptor Heap, Fence, Frame Resource, Upload Ring, Resource State tracking은 백엔드가 소유한다. 이를 `FSceneRenderer`나 Material API에 노출하지 않는다.

Command List Handle은 D3D11에서는 논리적인 기록 구간이고 D3D12에서는 실제 네이티브 Command List를 가리킬 수 있다. 상위 계층은 이 차이에 의존하지 않는다.

## Texture와 Pipeline 계약

### Texture usage와 view

Texture는 여러 용도로 사용될 수 있으므로 usage를 bit flag로 표현한다.

```cpp
enum class ETextureUsage : uint8
{
	None = 0,
	ShaderResource = 1 << 0,
	RenderTarget = 1 << 1,
	DepthStencil = 1 << 2,
};
```

Shadow Map은 `DepthStencil | ShaderResource` 용도를 가진다. Texture 자원과 SRV, RTV, DSV 역할은 `FTextureHandle`과 `FTextureViewHandle`로 구분한다.

D3D11은 생성 시 bind flag와 view를 만들고, D3D12는 resource와 descriptor를 만든다.

### Graphics Pipeline

Graphics Pipeline은 다음 상태를 하나의 Handle로 묶는다.

- Vertex와 Pixel Shader
- Vertex Layout과 Primitive Topology
- Blend State
- Rasterizer State
- Depth Stencil State
- Color/Depth Target format과 Sample Count

Pipeline output layout은 현재 rendering attachment와 호환되어야 한다. Shadow Pass를 위해 다음 조합을 허용한다.

- Color attachment 없음
- Pixel Shader 없음
- Depth attachment만 존재
- Color write 없음

Material은 Pass마다 적합한 Pipeline을 선택할 수 있다. Shadow Pass는 depth-only Pipeline을, Opaque Pass는 Material Pipeline을, Translucency Pass는 blend와 depth write가 다른 Pipeline을 사용한다.

### 상수와 Resource binding

`SetConstantData()`는 한 Draw가 사용할 작은 상수 데이터를 기록한다는 의미를 가진다.

- D3D11은 동적 Constant Buffer와 `Map(WRITE_DISCARD)`로 구현할 수 있다.
- D3D12는 프레임별 Upload Ring에서 새로운 영역을 할당하고 CBV를 바인딩한다.

Texture, Sampler, Material parameter binding도 같은 방식으로 API 중립 명령을 제공한다. Descriptor index나 네이티브 포인터는 상위 계층에 전달하지 않는다.

## Material과 Pipeline 선택

Material의 렌더링용 데이터는 Component 객체와 분리된 proxy로 표현한다.

```cpp
enum class EBlendMode : uint8
{
	Opaque,
	Masked,
	Translucent,
};

struct FMaterialRenderProxy
{
	EBlendMode BlendMode = EBlendMode::Opaque;
	bool bTwoSided = false;
};
```

각 Pass는 Material과 Mesh 정보를 사용해 Shader permutation과 Pipeline을 선택한다. Pipeline cache가 필요해지면 다음 값을 key로 사용한다.

```text
Shader permutation
Vertex layout
Pass type
Render target layout
Blend state
Depth stencil state
Rasterizer state
```

초기에는 Renderer가 소수의 Pipeline을 직접 소유해도 된다. Material 종류와 Pass 조합이 증가할 때 cache를 분리한다.

## Render Graph 도입 기준

고정 Pass 스케줄은 Shadow, Forward Opaque, Translucency, Overlay 정도의 구조에 충분하다.

다음 문제가 실제로 나타날 때 Render Graph 도입을 검토한다.

- 여러 Post Process Pass가 임시 Render Target을 공유함
- Pass 의존성과 Resource state transition을 수동으로 관리하기 어려움
- Render Target lifetime aliasing이 필요함
- Async Compute와 Graphics Queue 동기화가 필요함
- 여러 View와 렌더링 경로의 Pass 조합이 동적으로 변함

Render Graph를 도입하더라도 `FScene`, View별 가시 Primitive 수집, Pass별 Draw Command 생성과 RHI Handle 계약은 유지한다. Graph는 Pass 실행 순서와 Resource lifetime을 관리하는 상위 스케줄러로 추가한다.

## 스레딩과 수명

현재 Scene 갱신, 가시성 판정, Draw Command 생성과 RHI 호출은 메인 스레드에서 실행한다.

- Component는 자신이 소유한 Scene proxy 포인터를 Scene에 추가하고 변경을 알리며 파괴 전에 제거한다.
- `FSceneRenderer`는 한 프레임 동안 Scene proxy와 View 데이터를 읽는다.
- Draw Command는 해당 프레임의 Pass 실행이 끝날 때까지만 유지한다.
- GPU Buffer, Texture, Shader와 Pipeline은 Render Handle로 참조한다.
- `IRenderContext`의 Back Buffer view는 Resize와 다음 Acquire에 의해 변경될 수 있다.

Render Thread를 도입하면 Game 객체를 직접 참조하지 않고 Scene proxy 등록과 변경 명령을 전달한다. 제거 명령이 Render Thread에서 처리되기 전에는 Component가 Proxy를 파괴하지 않는다. 병렬 Command 생성을 도입하더라도 작업 스레드는 완성된 가시 Primitive 배열과 불변 render proxy만 읽어야 한다.

D3D12에서는 CPU 프레임 수명과 GPU 완료 시점이 다르므로 Frame Resource와 Fence를 백엔드가 관리한다. 상위 SceneRenderer의 프레임 데이터 수명과 GPU 자원 파괴 시점은 별도의 deferred release 정책으로 연결한다.

## 현재 구현 상태

### 구현됨

- `IRenderDevice`와 `IRenderContext` 책임 분리
- Index와 Generation으로 구성된 Render Handle
- D3D11 Buffer, Texture, Shader와 Graphics Pipeline 생성
- 논리 Command List 시작, 종료와 제출
- Vertex/Index Buffer와 Constant Data 설정
- Draw와 DrawIndexed 명령
- Blend, Rasterizer, Depth 상태를 포함하는 Graphics Pipeline
- D3D11 Swap Chain, Back Buffer, Depth Target과 Resize
- 프레임 Clear, Draw, ImGui Overlay와 Present 연결
- Mesh resource의 GPU 업로드와 수명 관리

### 미구현

- `FScene`의 Primitive와 Light proxy 포인터 등록
- `FSceneRenderer`와 View별 가시 Primitive 배열
- Frustum culling 결과의 Render Pass 공유
- `FMeshDrawCommand`와 Pass별 Sort Key
- Opaque와 Translucency Queue 분리
- 명시적인 `BeginRender()`와 `EndRender()`
- Texture View Handle과 복합 Texture usage
- Material render proxy와 Pipeline cache
- Shadow View, Shadow Map과 Shadow Pass
- Main View Light 선정과 lighting resource binding
- D3D12 Render Device와 Render Context
- Render Thread, 병렬 Command 생성과 deferred resource release
- Optional Depth Prepass, Deferred Light Pass와 Render Graph

## 관련 파일

- [Renderer.h](../KnotEngine/Source/Engine/Render/Renderer.h)
- [Renderer.cpp](../KnotEngine/Source/Engine/Render/Renderer.cpp)
- [Scene.h](../KnotEngine/Source/Engine/Render/Scene.h)
- [Scene.cpp](../KnotEngine/Source/Engine/Render/Scene.cpp)
- [RenderTypes.h](../KnotEngine/Source/Engine/Render/RHI/RenderTypes.h)
- [RenderDevice.h](../KnotEngine/Source/Engine/Render/RHI/RenderDevice.h)
- [RenderContext.h](../KnotEngine/Source/Engine/Render/RHI/RenderContext.h)
- [D3D11RenderDevice.h](../KnotEngine/Source/Engine/Render/D3D11/D3D11RenderDevice.h)
- [D3D11RenderDevice.cpp](../KnotEngine/Source/Engine/Render/D3D11/D3D11RenderDevice.cpp)
- [D3D11RenderContext.h](../KnotEngine/Source/Engine/Render/D3D11/D3D11RenderContext.h)
- [D3D11RenderContext.cpp](../KnotEngine/Source/Engine/Render/D3D11/D3D11RenderContext.cpp)
- [PrimitiveComponent.h](../KnotEngine/Source/Engine/Components/PrimitiveComponent.h)
- [PrimitiveComponent.cpp](../KnotEngine/Source/Engine/Components/PrimitiveComponent.cpp)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/EditorEngine.cpp)
- [ImGuiRenderBackend.h](../KnotEngine/Source/Editor/UI/ImGui/ImGuiRenderBackend.h)
- [Conventions.md](Conventions.md)
