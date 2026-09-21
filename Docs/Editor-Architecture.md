# Knot Engine Editor Architecture

## 문서 목적

이 문서는 Knot Engine Editor의 책임, 주요 객체의 소유 관계, 프레임 실행 순서와 Engine 계층 사이의 경계를 정의한다.

구체적인 UI 배치, 위젯 구현, 지원 명령과 조정 가능한 수치는 문서 계약으로 고정하지 않는다. 이러한 세부 사항은 구현에서 관리하고, 이 문서에는 기능이 확장되어도 유지해야 할 큰 흐름만 기록한다.

## 설계 원칙

- Editor는 Engine의 World, Reflection, Rendering과 Profiling 기능을 사용하는 상위 계층이다.
- Engine은 Editor의 Panel, Selection과 ImGui 타입을 참조하지 않는다.
- `UEditorEngine`은 Editor World와 프레임 실행을 조율하고, `FImGuiSystem`은 Editor UI를 조율한다.
- Panel은 구체 클래스로 두며 실제 공통 계약이 생기기 전에는 범용 Panel 기반 클래스나 등록 시스템을 만들지 않는다.
- Panel 사이의 상태 공유는 명시적인 상태 객체 또는 상위 조율자를 통한다.
- 일반 UI 입력은 ImGui가 처리하고, Viewport처럼 Engine 입력이 필요한 영역만 `FInputRouter`에 등록한다.
- World는 Viewport 수와 관계없이 WorldContext마다 한 번 Tick한다.
- Viewport는 자신의 offscreen Render Target에 World를 렌더링하고 ImGui가 그 결과를 합성한다.
- Inspector는 Reflection API로 값을 변경하고 `PostEditProperty()`로 변경 사실을 알린다.
- 기본 Dock layout은 초기 상태만 제공하며 이후에는 사용자가 저장한 ImGui layout을 따른다.

## 전체 구조

```text
FWindowsApplication
        ↓
UEditorEngine
├─ Editor WorldContext
├─ FRenderer
├─ FAssetRegistry
├─ FInputRouter
├─ FEditorViewportClient 목록
└─ FImGuiSystem
   ├─ FEditorSelection
   ├─ Hierarchy / Inspector
   ├─ Viewport
   ├─ Console / Profile / Content
   └─ Viewport Overlay
```

Editor의 한 프레임은 UI 구성, 입력 라우팅, World 갱신, Viewport 렌더링과 ImGui 합성으로 나뉜다.

```text
Input Snapshot
        ↓
ImGui UI 구성 및 Viewport 입력 대상 등록
        ↓
Editor 입력 라우팅
        ↓
World와 ViewportClient Tick
        ↓
ViewFamily별 Scene 렌더링
        ↓
ImGui 합성 및 Present
```

## 디렉터리와 책임

```text
KnotEngine/Source/Editor/
├─ Runtime/       Editor 시작, 종료와 프레임 조율
├─ Asset/         Content 스캔과 Editor Asset 메타데이터
├─ Input/         Editor 입력 대상과 소유권 라우팅
├─ Editor/        DockSpace, Asset Editor와 UI 구성
│  ├─ Panel/      독립 Editor 창
│  ├─ Widget/     Viewport와 Overlay 등 재사용하는 UI 조각
│  ├─ Toolbar/    공유 Toolbar
│  └─ Setting/    Editor 설정 데이터
└─ Viewport/      Viewport surface, Camera와 ViewportClient
```

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `UEditorEngine` | Editor World, Renderer, UI와 ViewportClient 실행 조율 | Panel별 위젯 구현 |
| `FImGuiSystem` | ImGui 수명, DockSpace, Panel 소유와 UI 프레임 구성 | World 렌더 패스 구현 |
| Panel | 한 Editor 창의 상태와 표시 | 다른 Panel의 수명 관리 |
| `FEditorSelection` | Panel이 공유하는 현재 선택 | 선택 객체의 소유권 |
| `FInputRouter` | Viewport 등 Engine 입력 대상 선택 | ImGui 위젯 처리, Win32 입력 수집 |
| `FViewportWidget` | Viewport surface 표시와 이미지 영역의 입력 등록 | Camera와 World 선택 정책 |
| `FViewport` | offscreen 출력 surface와 크기 | Camera와 World 선택 정책 |
| `FEditorViewportClient` | Camera 입력, View와 ViewFamily 구성 | Panel layout과 ImGui 렌더링 |

## 소유권과 수명

`UEditorEngine`은 Renderer를 참조하고 Input Router와 ImGui System을 소유한다. Editor World는 `UEngine`의 WorldContext를 통해 관리한다.

`FImGuiSystem`은 공유 Viewport Toolbar와 concrete Panel을 소유한다. Level Viewport Panel과 Asset Editor는 각각 `FViewportWidget`과 concrete ViewportClient를 소유하며, `UEditorEngine`은 등록된 ViewportClient를 non-owning 목록으로 순회한다. Selection은 `UEditorEngine`이 소유하고 필요한 Panel에 참조로 전달한다.

```text
FImGuiSystem
├─ 공유 Viewport Toolbar
└─ Viewport Panel / Asset Editor
        ├─ FViewportWidget
        └─ ViewportClient 실제 소유
                ↓ register / unregister
UEditorEngine
        └─ ViewportClient non-owning 순회
```

Selection도 객체 수명을 연장하지 않는다. 선택된 객체나 World가 사라지기 전에 선택 상태를 해제해야 한다.

Application은 EditorEngine과 ImGui System보다 오래 살아야 한다. 종료할 때는 외부 callback과 입력 참조를 먼저 끊고 UI 및 렌더링 자원을 해제한다.

## 프레임 실행 순서

`UEditorEngine`은 메인 스레드에서 다음 순서를 유지한다.

1. 완성된 `FInputSnapshot`을 `FInputRouter`에 전달한다.
2. ImGui frame을 시작하고 DockSpace와 Panel을 구성한다.
3. Viewport Panel이 실제 이미지 영역을 입력 대상으로 등록한다.
4. `FInputRouter`가 현재 프레임 입력을 대상별로 전달한다.
5. WorldContext마다 World를 한 번 Tick한다.
6. 등록된 ViewportClient를 Tick하고 ViewFamily를 구성한다.
7. ImGui draw data를 `FImGuiDrawDataCopy`로 복사하고 ViewFamily와 함께 Render Thread에 제출한다.
8. Render Thread가 Scene을 offscreen target에 렌더링하고 복사된 ImGui draw data를 Back Buffer에 합성한다.

UI 구성을 먼저 수행해야 현재 Viewport의 크기와 hover 상태를 알 수 있다. World 렌더링은 Viewport texture를 완성한 뒤 ImGui 합성보다 먼저 수행한다.

## DockSpace와 Panel

`FImGuiSystem`은 ImGui context와 backend의 수명, root DockSpace, Main MenuBar와 Panel의 Draw 호출을 관리한다.

각 Panel은 하나의 Editor 기능에 집중한다.

| Panel | 역할 |
|---|---|
| Hierarchy | 현재 World의 Level과 Node 구조 표시 및 선택 |
| Inspector | 선택 객체와 Component의 Reflection 프로퍼티 표시 및 편집 |
| Viewport | Scene 출력 표시, Camera 제어와 Engine 입력 대상 등록 |
| Console | Engine 로그 표시와 Console 명령 입력 |
| Profile | 완료된 CPU Profile 결과 표시 |
| Content | Content 폴더와 Asset 메타데이터 탐색 |

기본 Dock layout에서 오른쪽 영역은 Inspector가 사용하고, 아래쪽 영역은 Content, Console과 Profile이 같은 Dock node를 공유한다. Content를 첫 탭이자 기본 활성 탭으로 사용한다.

Panel visibility와 Dock layout은 UI 상태다. World와 객체 데이터의 저장 형식과 섞지 않는다.

`FAssetRegistry`는 Content 파일의 경로와 종류를 인덱싱하며 UObject나 GPU 리소스를 생성하지 않는다. `FAssetManager`는 요청된 Asset의 로드와 UObject 수명을 담당하고, Content Panel은 Registry의 스냅샷을 표시한다.

## Selection과 Inspector

Hierarchy가 선택 상태를 변경하면 Inspector는 같은 `FEditorSelection`을 읽는다. Hierarchy의 Add Node 메뉴는 `Empty`와 `EditorSpawnable` Component를 카테고리별로 표시한다. 생성한 Node는 Persistent Level에 추가되고 선택되며, Component 항목을 선택한 경우 기본 Transform과 해당 Component를 함께 가진다. Panel끼리 직접 호출하거나 서로를 소유하지 않는다.

Inspector의 Add Component 메뉴는 `FReflectionRegistry`가 등록 시점에 정렬한 `EditorSpawnable` 클래스 목록을 참조하고 `UComponent` 상속 관계를 검사한다. 메뉴 그룹과 이름은 `UCLASS`의 `Category`, `DisplayName` 메타데이터를 사용한다. Panel은 구체 Component 클래스를 직접 생성하지 않고 선택한 `UClass`를 `UNode::AddComponent()`에 전달한다.

Inspector의 값 변경은 다음 경로를 따른다.

```text
ImGui 편집
        ↓
FProperty를 통한 값 복사
        ↓
UObject::PostEditProperty
        ↓
Object가 파생 상태와 subsystem 변경을 예약
```

Inspector는 Engine의 public Reflection API만 사용한다. 편집 가능 여부는 Reflection metadata와 property flag로 결정하며 C++ 접근 지정자에 의존하지 않는다.

`PostEditProperty()`는 변경된 값을 바탕으로 cache나 dirty 상태를 갱신하는 통지 지점이다. Render 또는 Physics container를 순회하는 중에 위험한 구조 변경이 필요하면 각 subsystem의 안전한 시점으로 미룬다.

## Viewport와 Rendering

Viewport Panel은 ImGui 창 안에 `FViewport`의 offscreen texture를 표시한다. ViewportClient는 어떤 World와 Camera를 사용할지 결정하고 `FSceneViewFamily`를 만든다.

```text
Viewport Panel
        ↓ 크기와 Camera 상태
FEditorViewportClient
        ↓ FSceneViewFamily
FSceneRenderer
        ↓
Viewport Render Target
        ↓
ImGui 합성
```

SceneRenderer는 Panel이나 ViewportClient를 순회하지 않고 완성된 ViewFamily와 Scene data만 사용한다. Render Pass와 GPU 자원의 세부 계약은 [Rendering-Architecture.md](Rendering-Architecture.md)를 따른다.

Viewport 입력은 실제 Scene 이미지 영역만 대상으로 한다. Camera, 기즈모와 PIE 입력의 소유권 및 ImGui capture와의 관계는 [Input-Architecture.md](Input-Architecture.md)를 따른다.

## Console, Profile과 Overlay

Console은 Engine log sink가 전달한 메시지를 보관하고 표시한다. 로그 생산자는 Editor Panel을 알지 않으며, 다른 스레드에서 들어오는 메시지는 공유 저장소를 보호한 뒤 UI 스레드에서 읽는다.

Profile Panel은 Game Thread와 Render Thread별 CPU Profiler 완료 Snapshot 및 Render Thread가 발행한 GPU 통계를 읽는다. 수집 중인 profiler 상태나 Render Device를 UI가 직접 조회하지 않는다.

Viewport Overlay는 Viewport에 종속된 간단한 정보를 표시한다. Console은 표시 상태를 변경할 수 있지만 Overlay의 렌더링 자체를 담당하지 않는다. 통계 수집은 Engine/Core에 남고 Editor는 결과만 표현한다.

## 입력 시스템과의 경계

Editor는 플랫폼 메시지를 직접 처리하지 않는다. `FWindowsApplication`과 `FWindowsInput`이 만든 `FInputSnapshot`을 `FInputRouter`가 Editor 대상에 전달한다.

일반 ImGui Panel은 `IInputTarget`을 구현하지 않는다. Viewport, 기즈모와 PIE Game Viewport처럼 ImGui 바깥의 Engine 동작이 필요한 객체만 입력 대상으로 등록한다.

입력 데이터, capture, focus와 Down/Up 소유권의 구체 계약은 [Input-Architecture.md](Input-Architecture.md)를 따른다.

## 스레딩과 계층 경계

현재 UI 구성, Editor 입력 라우팅, World Tick과 Scene 제출은 메인 스레드에서 실행한다.

- Panel은 메인 스레드에서만 ImGui API를 호출한다.
- SceneRenderer는 Render Thread의 확정된 Scene proxy 상태를 읽는다.
- Worker thread에서 생성된 로그는 동기화된 경로를 통해 Console에 전달한다.
- ViewFamily와 ImGui draw data는 Render Command가 값을 소유하고 Render Target의 생성·교체·제거는 Render Thread FIFO에서 수행한다.

Editor 기능을 Engine에 추가하지 않는다. 여러 실행 환경에서 필요한 데이터 수집과 runtime 기능은 Engine에 두고, 선택·도킹·위젯·편집 정책은 Editor에 둔다.

## 확장 원칙

- 새로운 독립 창은 concrete Panel로 추가하고 `FImGuiSystem`이 소유한다.
- Panel 사이에 공유 상태가 필요하면 목적이 분명한 작은 상태 객체를 상위에서 소유한다.
- 같은 UI 조각이 반복될 때만 재사용 함수나 Widget을 추출한다.
- runtime 등록, 플러그인 확장 또는 공통 lifecycle 요구가 확인될 때만 Panel registry와 기반 클래스를 검토한다.
- Inspector customization과 Editor command도 실제로 여러 호출 지점에서 공유될 때 도입한다.
- Undo/Redo는 Reflection 값 변경 경로 위에 transaction 계층으로 추가하며 Inspector에 임시 복사 기능을 중복 구현하지 않는다.

## 현재 구현 상태

### 구현됨

- Editor WorldContext와 기본 World 실행
- ImGui context, DockSpace, Main MenuBar와 Panel 수명 관리
- Hierarchy 선택과 Reflection 기반 Inspector 편집
- Hierarchy와 Inspector의 Reflection 기반 Node·Component 생성 메뉴
- Viewport offscreen rendering과 Editor Camera
- Viewport 입력 라우팅
- Console log sink와 명령 입력
- CPU Profile 표시와 Viewport 통계 Overlay
- Content 스캔과 Asset Registry 기반 Content Panel
- 최대 2개 프레임의 비동기 Render Thread 제출
- ImGui draw data 깊은 복사와 Context를 읽지 않는 RT 합성
- Thread별 CPU Profile과 RT 발행 GPU 통계 표시

### 미구현

- Source Asset Import와 Reimport
- Component 단위 선택과 기즈모
- Undo/Redo와 범용 Editor command
- PIE Game Viewport

## 관련 문서

- [Input-Architecture.md](Input-Architecture.md)
- [Reflection-Architecture.md](Reflection-Architecture.md)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [World-Architecture.md](World-Architecture.md)
- [Conventions.md](Conventions.md)

## 관련 파일

- [EditorEngine.h](../KnotEngine/Source/Editor/Runtime/EditorEngine.h)
- [ImGuiSystem.h](../KnotEngine/Source/Editor/Editor/ImGuiSystem.h)
- [EditorSelection.h](../KnotEngine/Source/Editor/Editor/EditorSelection.h)
- [InputRouter.h](../KnotEngine/Source/Editor/Input/InputRouter.h)
- [Viewport.h](../KnotEngine/Source/Editor/Viewport/Viewport.h)
- [EditorViewportClient.h](../KnotEngine/Source/Editor/Viewport/EditorViewportClient.h)
