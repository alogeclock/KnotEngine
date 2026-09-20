# Knot Engine 렌더 스레드 분리 기획서

> 작성일: 2026-09-19 (Asia/Seoul)  
> 상태: 구현 전 설계·작업 계획  
> 범위: 게임/에디터 메인 스레드와 하나의 렌더 스레드 분리. RHI 스레드는 후속 고려 사항이다.

## 1. 결론

Knot Engine의 첫 렌더 스레드 구현에는 `FRenderSubmission`, `FRenderFramePacket`, `BeforeFrame`, `AfterFrame`, 범용 Resource Version 저장소가 필요하지 않다.

필요한 핵심 구성은 다음 세 가지다.

1. 렌더 명령을 FIFO 순서로 실행하는 `FRenderThread`
2. 렌더 스레드만 변경하는 기존 `FScene`과 `FPrimitiveSceneProxy`
3. UObject와 분리된 `FMaterialRenderProxy`

게임/에디터 스레드는 Scene 변경 명령과 프레임 렌더 명령을 발생 순서대로 enqueue한다. 렌더 스레드는 같은 큐에서 이를 순서대로 실행한다.

```text
Game / Editor Thread
  ├─ AddPrimitive 명령
  ├─ UpdatePrimitive 명령
  ├─ UpdateMaterial 명령
  ├─ ResizeViewport 명령
  └─ RenderViewFamilies 명령
                    │
                    ▼
          FRenderThread FIFO
                    │
                    ▼
Render Thread
  ├─ FScene 갱신
  ├─ FSceneRenderer 생성
  ├─ 가시성 수집과 Pass 정렬
  ├─ FRenderGraph 실행
  └─ D3D11 호출과 Present
```

FIFO 자체가 `BeforeFrame`과 `AfterFrame`의 순서를 표현한다. 별도 Submission 계층으로 같은 순서를 다시 모델링하지 않는다.

## 2. 언리얼에서 참고할 핵심 원칙

언리얼의 전체 Task Graph와 RHI 구조를 Knot에 복제하지 않는다. 현재 문제에 직접 필요한 다음 원칙만 채택한다.

### 2.1 렌더 명령은 하나의 순서화된 흐름이다

언리얼의 `ENQUEUE_RENDER_COMMAND`는 Scene 갱신, Resource 생성·해제, View Family 렌더링을 렌더 스레드에 순서대로 전달한다. 모든 명령을 다시 하나의 Frame Packet에 넣어야 실행되는 구조가 아니다.

확인 위치:

- `Engine/Source/Runtime/RenderCore/Public/RenderingThread.h`
- `Engine/Source/Runtime/Renderer/Private/RendererScene.cpp`
- `Engine/Source/Runtime/Renderer/Private/SceneRendering.cpp`

Knot도 하나의 FIFO 명령 큐를 사용한다. 다음 코드의 타입명과 세부 저장 방식은 의사 코드다.

```cpp
class FRenderThread final
{
public:
    void Start();
    void Stop();
    void Enqueue(std::function<void()> Command);
    void Flush();

private:
    void Run();
};
```

`std::function`을 사용할 수 없는 이동 전용 캡처가 실제로 필요해질 때만 큐 내부 구현을 이동 전용 호출 객체로 바꾼다. 이를 별도의 공개 Render Command 계층으로 만들 필요는 없다.

### 2.2 FScene은 렌더 스레드의 미러다

언리얼은 Component를 매 Draw마다 읽지 않는다. 게임 스레드에서 Scene Proxy와 갱신 값을 만들고 렌더 명령으로 전달한다. 렌더 스레드는 자기 `FScene`과 Proxy 상태만 읽는다.

Knot에서도 `FPrimitiveSceneProxy::Update()`가 Component를 다시 조회하는 현재 구조를 제거한다.

```text
UPrimitiveComponent [GT]
  └─ 변경 값을 복사
       └─ Enqueue Update
            └─ FPrimitiveSceneProxy::ApplyUpdate_RenderThread() [RT]
```

### 2.3 프레임 렌더러는 장기 객체일 필요가 없다

언리얼은 View Family 렌더 작업을 enqueue하고, 렌더 작업 범위에서 Scene Renderer를 사용한다. Knot의 `FSceneRenderer`도 렌더 스레드 스택에서 생성하고 같은 명령 안에서 `FRenderGraph` 실행까지 끝낸다.

따라서 `FSceneRenderer`, `FRenderGraph`, Pass를 GT와 RT 사이에 전달하는 장기 패킷으로 만들지 않는다.

### 2.4 Fence는 필요한 지점에서만 사용한다

언리얼의 `FRenderCommandFence`는 앞서 enqueue한 렌더 명령의 완료를 기다리는 도구다. 프레임 데이터의 필수 소유 컨테이너가 아니다.

Knot의 최초 구현은 `FRenderThread::Flush()` 하나로 충분하다. 비동기 완료 여부를 여러 시스템이 개별 조회해야 할 때만 별도 Fence 타입을 추가한다.

## 3. 제거하거나 미룰 추상화

| 기존 제안 | 결정 | 이유 |
|---|---|---|
| `FRenderSubmission` | 제거 | FIFO 큐가 이미 명령 순서를 보장한다. |
| `FRenderFramePacket` | 제거 | 한 프레임의 소유 값은 프레임 렌더 명령이 직접 캡처하면 된다. |
| `BeforeFrame` / `AfterFrame` | 제거 | enqueue 순서로 표현할 수 있다. |
| `ControlOnly Submission` | 제거 | Scene/Resource/Resize 명령은 프레임과 무관하게 같은 큐에서 실행한다. |
| 범용 `FRenderCommand` 클래스 계층 | 만들지 않음 | 큐 내부의 호출 객체면 충분하다. |
| `PrimitiveId -> SceneIndex` 맵 | 초기에는 만들지 않음 | 기존 Proxy 포인터를 GT에서는 불투명 식별자로만 보관할 수 있다. |
| `ViewportId` / `TargetRevision` | 초기에는 미룸 | Resize 뒤 `Flush()`하는 직렬 단계로 먼저 정확성을 검증한다. |
| 범용 Render Resource Version 저장소 | 초기에는 미룸 | 현재 Asset Manager의 장기 소유와 종료 전 Flush를 먼저 이용한다. |
| `Inline / ThreadedSerialized / Threaded` 모드 계층 | 단순 플래그로 대체 | 같은 `Enqueue()`가 즉시 실행할지 큐에 넣을지만 바꾸면 된다. |
| 범용 `FUIRenderPacket` | 만들지 않음 | 실제 중첩 단계에서 ImGui에 필요한 구체 데이터 복사 타입만 추가한다. |

이 항목들은 영구히 금지하는 것이 아니다. 실제 요구가 생긴 시점에 해당 문제를 해결하는 가장 작은 타입을 추가한다.

## 4. 최소 소유 구조

### 4.1 FRenderThread

`FRenderThread`는 새로 추가하는 유일한 실행 계층이다.

담당 책임:

- 하나의 Worker Thread 시작과 종료
- Mutex와 Condition Variable 기반 FIFO 큐
- 렌더 명령 실행
- `Flush()` 완료 대기
- 렌더 스레드 소속 검증
- 최대 미완료 렌더 프레임 수 제한
- 실패와 종료 시 대기자 깨우기

담당하지 않는 책임:

- Scene 데이터 모델
- Pass 생성과 정렬
- Render Graph 작성
- Asset 로딩
- Material 해석
- RHI Command Stream 기록

`FRenderThread`는 `URenderer`를 대체하지 않는다. 기존 렌더러 함수를 어느 스레드에서 실행할지만 통제한다.

### 4.2 FScene

기존 `FScene`을 유지하고 내부 상태는 RT 전용으로 만든다.

- Proxy 추가, 갱신, 제거는 RT에서만 수행한다.
- `GetProxies()`는 렌더 경로에서만 사용한다.
- `FSceneRenderer`가 RT에서 가시 Proxy를 수집한다.
- GT Picking은 FScene Proxy를 순회하지 않는다.

`UWorld`는 당분간 현재처럼 Scene의 논리적 소유자다. 첫 구현에서는 World 파괴 전에 `Flush()`하여 Scene을 참조하는 명령이 남지 않도록 한다. 이 방법으로 충분한 동안 `RetireScene` 명령과 지연 Scene 소유권 이전을 도입하지 않는다.

향후 World를 렌더링 중에도 비동기로 즉시 파괴해야 할 요구가 생기면 그때 Scene 지연 폐기를 추가한다.

### 4.3 FPrimitiveSceneProxy

Proxy는 생성 후 RT 상태다. GT의 Component를 참조하지 않는다.

```cpp
struct FPrimitiveSceneProxyData
{
    FMatrix WorldMatrix;
    FBox LocalBounds;
    bool bVisible = true;
    bool bSelected = false;
};
```

위 타입은 별도 시스템이 아니라 Component와 Proxy 사이에서 복사할 최소 값 묶음이다. Static Mesh 전용 데이터가 필요하면 `FStaticMeshSceneProxyData`처럼 실제 Proxy 단위로 추가한다. 모든 Primitive를 포괄하는 범용 Property Bag은 만들지 않는다.

```text
OnRegister [GT]
  → Component 값으로 Proxy 생성
  → AddPrimitive 명령 enqueue
  → RT의 FScene이 Proxy 소유

속성 변경 [GT]
  → Dirty 표시
  → 현재 값을 Data로 복사
  → ApplyUpdate 명령 enqueue

OnUnregister [GT]
  → RemovePrimitive 명령 enqueue
  → RT에서 Scene 제거 및 Proxy 파괴
```

Component가 보관하는 Proxy 포인터는 명령 대상을 식별하는 용도로만 사용한다. GT에서는 역참조하지 않는다. Add, Update, Remove가 같은 FIFO에 있으므로 순서가 유지된다.

포인터 재사용이나 외부 생산자 때문에 이 계약을 지키기 어려워지는 시점에만 Generation이 포함된 Primitive Handle을 추가한다.

### 4.4 FMaterialRenderProxy

`FMaterialRenderProxy`는 유지해야 하는 추상화다. 이것은 단순 포장 타입이 아니라 UObject와 RT 사이의 실제 소유 경계를 만든다.

```text
UMaterialInterface [GT]
  ├─ Parent와 Override 해석
  └─ 렌더에 필요한 최종 값 작성
                  │ Update 명령
                  ▼
FMaterialRenderProxy [RT]
  ├─ Shader Key
  ├─ Blend / Depth / Cull 상태
  ├─ Material Constant 값 또는 Packing Cache
  └─ Texture / Sampler 렌더 참조
```

`FSceneRenderer::Prepare()`와 `FOpaquePass`는 `UMaterialInterface`, `UMaterial`, `UTexture`를 직접 조회하지 않는다. `FMaterialRenderProxy`에서 이미 해석된 렌더 상태만 읽는다.

다만 최초 구현에서 Material Revision 시스템, 범용 Dependency Graph, 다중 Device별 Proxy 저장소까지 만들지 않는다. Material 변경 시 해당 Proxy 갱신 명령을 enqueue하는 것으로 시작한다.

## 5. 프레임 데이터는 별도 RenderFrame 없이 전달한다

프레임 렌더 자체를 하나의 렌더 명령으로 취급한다.

```cpp
void UEditorEngine::Render()
{
    TArray<FSceneViewFamily> ViewFamilies = BuildViewFamilies();
    FDebugDrawData DebugDrawData = DebugDraw.Consume();

    RenderThread.Enqueue([
        this,
        ViewFamilies = std::move(ViewFamilies),
        DebugDrawData = std::move(DebugDrawData)]() mutable
    {
        RenderViewFamilies_RenderThread(ViewFamilies, DebugDrawData);
    });
}
```

렌더 명령이 값 캡처한 `ViewFamilies`와 `DebugDrawData`가 그 프레임의 소유 데이터다. 이를 다시 `FRenderFramePacket`에 넣어도 수명은 더 안전해지지 않는다.

`RenderViewFamilies_RenderThread()`는 다음 순서로 동기 실행한다.

1. `URenderer::BeginFrame()`
2. 각 View Family마다 지역 `FSceneRenderer` 생성
3. 가시성 수집
4. Pass 명령 생성과 정렬
5. 지역 `FRenderGraph` 실행
6. Debug Draw와 UI 합성
7. `URenderer::EndFrame()`과 Present

이 함수가 반환되면 지역 `FSceneRenderer`, `FRenderGraph`, Pass 명령은 모두 폐기된다.

### 프레임 제한

전체 큐 크기를 2로 제한하지 않는다. Scene 제거와 Resource 해제 같은 제어 명령까지 막힐 수 있기 때문이다.

대신 `FRenderThread` 안에 제출한 렌더 프레임 수와 완료한 렌더 프레임 수만 둔다.

```text
SubmittedRenderFrames - CompletedRenderFrames < MaxFramesInFlight
```

초기 `MaxFramesInFlight`는 1로 두어 직렬 동작을 검증한다. UI 데이터 복사가 끝난 뒤 2로 올려 GT와 RT를 중첩한다.

## 6. 명령 순서 예시

### 6.1 Primitive 변경 후 렌더

```text
Enqueue AddPrimitive
Enqueue UpdatePrimitive
Enqueue RenderViewFamilies
Enqueue RemovePrimitive
```

RT는 위 순서를 그대로 실행한다. 별도 `BeforeFrame`과 `AfterFrame` 배열은 필요하지 않다.

### 6.2 Texture 생성 후 Material 사용

```text
Enqueue InitTextureResource
Enqueue UpdateMaterialRenderProxy
Enqueue RenderViewFamilies
```

같은 FIFO를 사용하므로 Material이 Texture 생성보다 먼저 실행되지 않는다.

### 6.3 종료

```text
GT 신규 작업 중단
→ RemovePrimitive / ReleaseResource enqueue
→ Flush
→ RT에서 Renderer와 D3D 자원 종료
→ RT Stop / Join
→ World, Asset, Window의 CPU 객체 파괴
```

초기에는 이 명시적인 Flush가 복잡한 지연 폐기 시스템보다 안전하고 이해하기 쉽다.

## 7. Mesh와 Texture는 현재 수명을 우선 활용한다

최초 RT 분리를 위해 즉시 `shared_ptr<const FStaticMeshRenderData>`와 Device별 Resource Version 저장소를 도입하지 않는다.

현재 Knot의 Asset은 `FAssetManager`가 장기 보관한다. 이 특성을 이용해 다음 제약을 먼저 둔다.

- 로드가 끝난 Mesh 정점·인덱스·Section 데이터는 실행 중 제자리 수정하지 않는다.
- Texture Mip 데이터도 GPU 생성이 끝날 때까지 변경하거나 파괴하지 않는다.
- GPU 생성과 해제는 RT 명령으로만 수행한다.
- Asset Manager 종료 전 `Flush()`한다.
- Asset Hot Reload는 기존 데이터를 수정하지 않고 새 Asset/Render Resource로 교체한다.

이 제약으로 초기 수명을 해결할 수 있다. 런타임 개별 Asset Unload나 여러 Device가 실제 요구될 때 불변 Render Data 공유와 지연 해제를 추가한다.

`FStaticMeshLOD`의 CPU 데이터와 `FMeshBuffer`, `UTexture2D`의 Mip과 GPU Texture를 물리적으로 분리하는 작업은 유용하지만, 렌더 스레드 큐를 만들기 위한 선행 추상화로 강제하지 않는다. 우선 모든 GPU 접근의 스레드 소속을 RT로 통일한다.

## 8. Picking, Viewport, ImGui, DebugDraw

### 8.1 CPU Picking

`FLevelEditorViewportClient::Raycast()`는 더 이상 RT Proxy를 갱신하거나 `GetOwner()`를 호출하지 않는다. GT의 World, Component Transform, Static Mesh CPU Geometry를 사용한다.

이는 별도 Picking Scene을 만들겠다는 뜻이 아니다. 현재 O(N) Triangle 순회를 GT 데이터로 옮기는 작업이다.

### 8.2 Viewport Resize

첫 단계에서는 Resize 명령을 RT에 enqueue하고 즉시 `Flush()`한다. Resize 빈도가 성능 문제로 확인되기 전에는 `ViewportId`, `TargetRevision`, 여러 세대의 Target Set을 만들지 않는다.

정상 프레임 도중에는 GT가 Render Target을 생성·해제하지 않는다. Display Texture Handle을 GT UI가 읽어야 하는 현재 구조는 직렬 단계에서 완료 후 읽는다.

실제 GT/RT 중첩 단계에서 Resize 중에도 이전 Target을 표시해야 한다면 그때 두 세대의 Target만 유지하는 구체적인 구조를 추가한다.

### 8.3 ImGui

전용 RT만 만들었다고 즉시 GT와 RT를 중첩하지 않는다.

1. 먼저 매 프레임 렌더 명령 뒤 `Flush()`하여 기존 ImGui 경로를 직렬로 검증한다.
2. 그 다음 ImGui 정점, 인덱스, Draw Command, Clip Rect를 깊은 복사하는 구체 타입을 만든다.
3. RT 백엔드는 복사 데이터만 소비하고 GT의 ImGui Context를 읽지 않는다.
4. 이 단계가 끝난 뒤 최대 미완료 프레임 수를 2로 올린다.

타입명은 구현 시 실제 역할을 드러내는 `FImGuiDrawDataCopy` 정도가 적절하다. 범용 `FUIRenderPacket`이나 UI 명령 프레임워크는 만들지 않는다.

### 8.4 DebugDraw

현재 `FDebugDraw`의 제출 배열은 GT 프레임 데이터이고 GPU Shape Mesh와 Buffer는 RT 자원이다.

- GT: Line과 Instance 배열 작성 후 렌더 명령에 값으로 이동
- RT: 전달된 배열을 현재 GPU 캐시에 업로드하고 Draw

이를 위해 필요한 것은 작은 `FDebugDrawData` 값 타입뿐이다. 별도 Debug Draw Manager나 Render Packet은 필요하지 않다.

## 9. D3D11과 RHI 스레드

현재 D3D11 `FCommandListHandle`은 Immediate Context 사용 구간을 나타내며 명령을 저장하는 버퍼가 아니다. 따라서 Handle을 다른 스레드로 넘겨도 RHI 스레드가 구현되는 것은 아니다.

이번 단계에서는 다음 호출을 모두 RT에 둔다.

- Device/Context를 사용하는 GPU Resource 생성과 해제
- Shader와 Pipeline State 생성
- Constant Buffer 갱신
- Render Target Resize
- Draw
- Submit과 Present

RHI 스레드는 실제 프로파일링에서 RT의 네이티브 API 호출이 병목일 때 별도 작업으로 진행한다.

```text
현재
GT → Render Command FIFO → RT [Scene + Pass + D3D11]

후속
GT → Render Command FIFO → RT [Scene + Pass + RHI 명령 기록]
                              → RHI Thread [D3D 실행]
```

후속 RHI Thread에서는 실행할 모든 상수 바이트와 Resource 참조를 Command Stream이 소유해야 한다. 이 요구가 생긴 시점에는 `FRHICommandStream` 같은 새 계층이 정당하다. 지금 미리 빈 계층만 만들지 않는다.

## 10. 구현 단계

### P0. 기준선과 스레드 검증

- 현재 렌더 결과와 Draw 수 기록
- `IsInGameThread()`와 `IsInRenderingThread()` 추가
- 렌더 관련 GPU 호출에 RT 검증 지점 지정
- `FRenderThread`는 아직 Inline 실행

완료 조건: 기존 결과가 달라지지 않고 어떤 코드가 GT/RT 소속인지 확인할 수 있다.

### P1. Primitive Proxy의 Component 의존 제거

- 생성 시 Component 값을 Proxy에 복사
- `FPrimitiveSceneProxyData`와 `ApplyUpdate_RenderThread()` 추가
- `bDirty`, Selection 변경은 GT에서 갱신 명령 생성
- `GetOwner()`와 Component 참조 제거
- CPU Picking을 GT Component 기반으로 변경

완료 조건: `FScene::Update()`와 Proxy가 Component/UObject를 읽지 않는다.

### P2. Material Render Proxy 도입

- `FMaterialRenderProxy` 추가
- GT에서 Parent와 Override 최종값 계산
- RT에서 Shader/Layout/상수/Texture 바인딩 준비
- `SceneRenderer::Prepare()`와 `OpaquePass`의 `UMaterialInterface` 접근 제거

완료 조건: 렌더 명령 실행 중 Material UObject를 조회하지 않는다.

### P3. 실제 렌더 스레드와 직렬 실행

- `FRenderThread` Worker, FIFO, `Flush()`, Stop/Join 구현
- Add/Update/Remove/Resource/Resize/Render를 같은 큐로 이동
- 모든 D3D11 호출을 RT로 이동
- 매 프레임 `Flush()`하여 결과와 종료 순서 검증

완료 조건: D3D Debug Layer와 스레드 검증 오류 없이 Inline 결과와 일치한다.

### P4. ImGui와 DebugDraw 데이터 소유

- `FImGuiDrawDataCopy` 구현
- `FDebugDrawData`를 프레임 명령에 값으로 전달
- CPUProfiler 수집 상태를 스레드별로 분리하고 완료 Snapshot만 UI에 전달

완료 조건: RT가 ImGui Context, Panel, GT 작성 배열을 접근하지 않는다.

### P5. GT/RT 중첩

- 매 프레임 `Flush()` 제거
- 최대 미완료 렌더 프레임 수를 2로 설정
- FrameId, 제출 대기, RT 실행 시간을 계측
- Resize, World 종료, Asset 종료 같은 수명 경계에서만 Flush

완료 조건: GT가 N+1 Frame을 작성하는 동안 RT가 N Frame을 안전하게 렌더링한다.

### P6. 측정 후 필요한 기능만 추가

다음 기능은 실제 요구나 병목이 확인될 때만 추가한다.

- Primitive Generation Handle
- 비동기 Scene 지연 폐기
- Asset별 Render Data Revision과 지연 GPU Release
- Resize 중 Target 세대 유지
- 장기 Mesh Draw Command Cache
- RHI Command Stream과 RHI Thread

## 11. 첫 구현의 클래스와 파일

새 파일은 다음 정도로 제한한다.

```text
Engine/Render/Thread/
  RenderThread.h
  RenderThread.cpp

Engine/Render/Proxy/
  PrimitiveSceneProxyData.h
  MaterialRenderProxy.h
  MaterialRenderProxy.cpp
```

`FDebugDrawData`는 기존 `DebugDraw.h`, ImGui 복사 데이터는 기존 Editor/Renderer ImGui 파일 가까이에 둔다. 별도의 `RenderFrame.h`, `RenderSubmission.h`, `RenderResourceStore.h`, `RenderSceneInterface.h`는 만들지 않는다.

## 12. 최종 불변식

아래 조건을 만족하면 첫 렌더 스레드 분리는 완료다.

- GT는 RT Proxy와 GPU Resource를 직접 수정하지 않는다.
- RT는 Component, UWorld 상태, Material UObject, ImGui Context를 조회하지 않는다.
- Scene 변경과 프레임 렌더 순서는 하나의 FIFO 큐로 설명할 수 있다.
- `FSceneRenderer`와 `FRenderGraph`는 한 RT 렌더 명령 안에서 생성되고 소멸한다.
- World와 Asset 종료 전 관련 렌더 명령을 `Flush()`한다.
- CPU 명령 완료와 GPU 완료를 같은 의미로 사용하지 않는다.
- Inline과 전용 RT의 동일 Frame 결과가 일치한다.
- 종료와 실패 시 큐 대기자와 Worker Thread가 남지 않는다.

이 설계의 핵심은 프레임을 새로운 객체 계층으로 표현하는 것이 아니라, **원본 객체를 읽지 않는 렌더 명령과 RT 전용 Scene을 만드는 것**이다.
