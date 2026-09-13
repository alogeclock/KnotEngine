# Knot Engine Editor Architecture

## 문서 목적

이 문서는 Knot Engine Editor의 ImGui Docking 화면, Panel 책임, 선택 상태, Inspector 프로퍼티 편집, Viewport 렌더링과 입력 라우팅 구조를 정의한다.

현재 구현 범위는 다음 네 dockable Panel과 하나의 전역 MenuBar다. Content Panel, Component 선택, Undo/Redo는 구현하지 않는다.

```text
Dockable Panel
├─ Hierarchy
├─ Inspector
├─ Viewport
└─ Console

Main MenuBar
└─ Navigation
```

`Navigation`은 독립된 dockable Panel이나 Widget 객체가 아니라 `FImGuiSystem`이 그리는 Main MenuBar다.

Inspector는 [Reflection-Architecture.md](Reflection-Architecture.md)의 스키마를 열거하고 값을 수정한다. Component는 에디터 파라미터마다 Setter를 제공하지 않는다. Inspector가 `FProperty` 연산으로 값을 반영한 뒤 `UObject::PostEditProperty()`를 호출하여 파생 객체가 cache, Transform, Render state와 같은 파생 상태를 갱신한다.

입력 수집과 Panel/Viewport 경계는 [Input-Architecture.md](Input-Architecture.md)의 `FInputRouter` 계약을 따른다.

## 설계 원칙

- `FImGuiSystem`은 DockSpace와 Panel의 생성, 수명 및 프레임 Draw를 조율한다.
- 네 창은 이름에 `Panel`을 사용하고 Main MenuBar는 `FImGuiSystem`이 직접 구성한다.
- `Widget`은 프로퍼티 행, Asset tile처럼 Panel 안에서 재사용되는 작은 UI 단위에만 사용한다.
- 첫 구현에서 `IEditorPanel`, Panel registry와 범용 Widget framework를 만들지 않는다.
- Panel은 서로를 직접 참조하지 않고 `FEditorSelection`과 명시적으로 전달받은 Editor 상태를 사용한다.
- Editor 모듈이 Engine의 public Reflection API를 사용하며 Engine은 Editor 타입을 참조하지 않는다.
- Inspector는 `FPropertyEditor` 기반 클래스나 friend 접근 게이트를 사용하지 않는다.
- Inspector의 값 변경은 `FProperty::CopyValue()`를 거친 뒤 `PostEditProperty()`로 알린다.
- `NoEdit` 프로퍼티는 UI에서 제외하고 C++ 접근 지정자는 편집 가능 여부로 사용하지 않는다.
- 일반 ImGui 창 입력은 ImGui가 처리하고 Viewport와 기즈모의 엔진 입력만 `FInputRouter`에 등록한다.
- World Tick은 Context마다 한 번 수행하고 Viewport 수에 따라 반복하지 않는다.
- Viewport는 Back Buffer에 직접 World를 그리지 않고 자신의 offscreen Render Target을 표시한다.
- 기본 Dock layout은 최초 한 번만 만들고 이후에는 ImGui ini 설정을 존중한다.

## 명명 규칙

| UI 단위 | 이름 | 이유 |
|---|---|---|
| Dockable 창 | `FHierarchyPanel` | 독립적으로 열고 닫거나 Dock할 수 있음 |
| Dockable 창 | `FInspectorPanel` | 선택 객체의 상세 정보를 표시함 |
| Dockable 창 | `FViewportPanel` | 엔진 Viewport를 ImGui 창에 배치함 |
| Dockable 창 | `FConsolePanel` | 로그 목록과 필터를 소유함 |
| Main MenuBar | `FImGuiSystem::DrawMenuBar()` | 창이 아니라 DockSpace 상단 navigation임 |
| 재사용 UI 조각 | `DrawFloatProperty()` 등 | 첫 구현은 함수로 충분함 |

Panel 공통 기반 클래스는 다음 요구가 둘 이상 나타날 때만 도입한다.

- 런타임 Panel 등록과 제거
- 공통 직렬화가 필요한 Panel ID
- 플러그인에서 Panel 추가
- 공통 lifecycle 또는 command binding

현재는 concrete 멤버와 명시적 `Draw()` 호출이 더 단순하다.

## 목표 디렉터리

```text
KnotEngine/Source/Editor/
├─ Runtime/
│  ├─ EditorEngine.h/.cpp
│  └─ Launch.h/.cpp
├─ Input/
│  └─ InputRouter.h/.cpp
├─ ImGui/
│  ├─ ImGuiSystem.h/.cpp
│  ├─ EditorSelection.h
│  └─ Panels/
│     ├─ HierarchyPanel.h/.cpp
│     ├─ InspectorPanel.h/.cpp
│     ├─ ViewportPanel.h/.cpp
│     └─ ConsolePanel.h/.cpp
└─ Viewport/
   ├─ Viewport.h/.cpp
   ├─ EditorViewportCamera.h/.cpp
   ├─ EditorViewportClient.h/.cpp
   ├─ Asset/
   │  └─ AssetEditorViewportClient.h/.cpp
   └─ Level/
      └─ LevelEditorViewportClient.h/.cpp
```

Inspector 코드가 커지면 다음 파일을 추가한다.

```text
Editor/ImGui/Property/
├─ PropertyWidgets.h/.cpp
└─ PropertyValueBuffer.h/.cpp       문자열 편집 상태가 필요할 때
```

프로퍼티 종류별 추상 drawer와 등록 map은 처음부터 만들지 않는다. `FInspectorPanel::DrawProperty()`의 type switch와 구체 함수로 시작하고 customization 요구가 확인될 때 분리한다.

## 전체 소유 구조

```text
UEditorEngine
├─ FWorldContext[]                 UEngine 소유
├─ URenderer
├─ FInputRouter
├─ FEditorViewportClient*[]        non-owning 순회 목록
└─ FImGuiSystem
   ├─ FEditorSelection
   ├─ FHierarchyPanel
   ├─ FInspectorPanel
   ├─ FViewportPanel
   │  ├─ FViewport
   │  └─ FLevelEditorViewportClient
   └─ FConsolePanel
```

`FImGuiSystem`은 Panel을 값 멤버로 소유한다. `FViewportPanel`은 자신의 출력 surface와 concrete ViewportClient를 함께 소유하지만 `UEditorEngine`을 참조하지 않는다. `FImGuiSystem`이 생성과 소멸 시 client를 `UEditorEngine`의 non-owning 순회 목록에 등록하고 해제한다. Panel 객체는 Editor 종료까지 주소가 안정적이므로 ViewportClient 같은 `IInputTarget`도 해당 프레임의 `RouteInput()`까지 안전하게 살아 있다.

```text
FImGuiSystem 생성
    └─ UEditorEngine::RegisterViewportClient
            ↓
UEditorEngine::AllViewportClients       non-owning Tick/ViewFamily 구성 순회
            ↓
FImGuiSystem 소멸
    └─ UEditorEngine::UnregisterViewportClient
```

`UEditorEngine`은 concrete Level viewport를 직접 소유하지 않는다. 등록 목록은 client의 수명을 연장하지 않으며 실제 수명은 Panel이 책임진다.

`FImGuiSystem`은 `UEditorEngine`을 참조하고, 필요한 Editor 상태를 해당 엔진에서 조회한다. World는 매 프레임 `Draw()`의 매개변수로 전달하지 않는다.

```cpp
void FImGuiSystem::Draw(float DeltaTime);
```

`FEditorContext`나 service locator 구조체는 만들지 않는다. 전달할 서비스가 실제로 늘어나 함수 계약이 불분명해질 때 작은 context 타입을 검토한다.

## Application과 ImGui 수명

Launch는 Application을 준비하고 그 참조로 EditorEngine을 생성한 뒤 Startup을 호출한다. EditorEngine은 FImGuiSystem을 직접 소유하며 UI 프레임 API를 호출한다. 별도 EditorUISystem이나 UI 추상 인터페이스는 없다.

FImGuiSystem은 생성자에서 `FWindowsApplication&`를 받아 보관하고 `Startup()`에서 ImGui context·backend를 초기화한 뒤 메시지 콜백을 등록한다. `Shutdown()`은 먼저 콜백을 해제한 뒤 UI 자원을 정리한다. Application은 EditorEngine과 FImGuiSystem보다 오래 살아야 한다. 초기화 상태는 bStarted로 구분한다.

Launch의 메시지 연결 코드와 EditorEngine의 ProcessWindowMessage는 없다. WindowsApplication은 일반 함수 포인터 콜백만 실행하며 ImGui 타입을 알지 않는다.

## DockSpace

### 초기화

`FImGuiSystem::Startup()`에서 ImGui context를 생성한 직후 Docking을 활성화한다.

```cpp
ImGuiIO& IO = ImGui::GetIO();
IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
```

현재 사용 중인 ImGui 배포본에 Docking API가 없다면 Docking branch 또는 Docking이 포함된 stable release로 의존성을 먼저 갱신한다. Editor 코드에서 자체 splitter나 창 배치 시스템으로 대체하지 않는다.

### Root DockSpace

매 프레임 Main Viewport의 작업 영역에 `DockSpaceOverViewport()`를 만들고 Main MenuBar를 먼저 그린다.

```text
Main ImGui Viewport
├─ Main MenuBar
└─ DockSpace
   ├─ Hierarchy
   ├─ Inspector
   ├─ Viewport
   └─ Console
```

개별 Panel은 일반 `ImGui::Begin()` 창이므로 자유롭게 Dock하거나 분리할 수 있다.

### 기본 배치

첫 실행 또는 저장된 Dock node가 없을 때만 DockBuilder로 기본 배치를 만든다.

```text
┌──────────────┬──────────────────────────┬──────────────┐
│ Hierarchy    │ Viewport                 │ Inspector    │
│              │                          │              │
│              ├──────────────────────────┤              │
│              │ Console                  │              │
└──────────────┴──────────────────────────┴──────────────┘
```

권장 비율은 왼쪽 20%, 오른쪽 25%, 아래 25%다. 정확한 픽셀 크기는 저장하지 않는다. ImGui ini 파일이 존재하면 사용자가 만든 layout을 덮어쓰지 않는다.

Panel 창 제목은 ImGui ini의 식별자로 사용되므로 안정적으로 유지한다. 표시 이름을 바꿀 필요가 있으면 `Visible Title###StablePanelId` 형식을 사용한다.

## Main MenuBar

현재 MenuBar는 `Window` 메뉴와 프레임 통계를 표시한다.

```text
Window
```

- `Window`: 네 Panel의 표시 bool을 토글한다.
- 메뉴 오른쪽 영역: FPS와 frame time을 표시한다.

첫 구현에서 command framework를 만들지 않는다. Menu item이 `FImGuiSystem`의 명시적인 함수 또는 Panel visibility bool을 변경한다. 동일 command를 MenuBar, 단축키와 Context Menu에서 함께 사용해야 할 때 `FEditorCommand`를 추출한다.

## FEditorSelection

Hierarchy와 Inspector는 선택 상태를 공유한다.

```cpp
struct FEditorSelection
{
	UNode* SelectedNode = nullptr;
};
```

초기 선택 포인터는 non-owning이다. Selection이 객체 수명을 늘리지 않는다. 다음 시점에는 선택을 먼저 해제한다.

- 선택된 Node 파괴 전
- 표시 WorldContext 교체 전
- Level unload 전
- Editor World 파괴 전

객체 파괴 알림 경로가 추가되면 Selection이 해당 알림을 받아 stale pointer를 제거한다. 현재 UUID와 InternalIndex는 객체 파괴 후 자동으로 무효화되는 weak handle이 아니므로 선택 handle처럼 사용하지 않는다.

Hierarchy가 Node를 선택하면 Inspector는 Node 자체 정보와 소유 Component들을 함께 표시한다. Component header는 프로퍼티 그룹일 뿐 선택 대상이 아니다.

## FHierarchyPanel

Hierarchy는 현재 World의 Level과 Node 소유 구조를 표시한다. Transform parent-child 계층과 Level 소속을 혼동하지 않는다.

첫 구현은 Level 아래에 모든 Node를 평탄하게 보여 준다.

```text
Editor World
├─ Persistent Level
│  ├─ Camera
│  ├─ Cube
│  └─ Light
└─ Loaded Level
   └─ Node
```

Transform hierarchy를 계층 Tree로 표시할 때도 Level 소속은 별도 root 또는 label로 보존한다. 다른 Level의 Transform 부착을 허용하지 않는 현재 정책에서는 Level마다 Transform root를 계산할 수 있다.

책임은 다음으로 제한한다.

- Level과 Node 열거
- 선택 변경
- Node 생성, 복제와 삭제 command 진입점
- drag-and-drop reparent가 구현되면 Transform parent 변경 요청

Hierarchy가 Inspector를 직접 호출하지 않는다. `FEditorSelection`만 변경한다.

## FInspectorPanel

Inspector는 선택된 UObject의 실제 `UClass`를 사용해 편집 가능한 프로퍼티를 열거한다.

```text
Selected UObject
	↓ GetClass()
UClass
	↓ GetEditorProperties()
TArray<const FProperty*>
	↓ Category / DisplayName / Tooltip
ImGui Property Rows
```

Node를 선택한 경우 다음 순서로 표시한다.

```text
Node 이름과 공통 정보
Transform Component
나머지 Components 배열 순서
```

각 Component는 collapsing header를 사용하고 header 안에서 해당 Component의 프로퍼티를 열거한다. Component 제거와 추가 UI는 Component 생명주기 및 reflection 생성 API가 준비된 뒤 추가한다.

### 프로퍼티 표시

`UStruct::GetEditorProperties()`는 부모 타입부터 현재 타입까지 선언 순서로 프로퍼티를 반환하고 `NoEdit`을 제외한다. Inspector는 결과를 metadata의 Category로 묶는다.

| Metadata | Inspector 사용 |
|---|---|
| `DisplayName` | 프로퍼티 label |
| `Category` | 접을 수 있는 그룹 |
| `Tooltip` | label 또는 편집 item hover 도움말 |

ImGui ID는 표시 문자열과 분리한다.

```cpp
ImGui::PushID(&Object);
ImGui::PushID(&Property);
ImGui::PushID(ArrayIndex);
```

같은 이름의 상속 프로퍼티, 여러 Component와 배열 원소가 충돌하지 않도록 객체, 프로퍼티 주소와 인덱스를 함께 사용한다.

### 프로퍼티 종류 식별

현재 `FProperty`에는 Inspector가 구체 종류를 질의하는 public API가 없다. C++ RTTI와 `dynamic_cast`에 의존하지 않도록 Engine Reflection에 작은 enum을 추가한다.

```cpp
enum class EPropertyKind : uint8
{
	Int32,
	Bool,
	Float,
	Double,
	String,
	Name,
	Enum,
	Struct,
	Object,
	SoftObject,
	Array
};
```

`FProperty::GetKind()`를 virtual query로 제공하고 각 concrete Property가 상수 값을 반환한다. 이 enum은 Editor Widget 종류가 아니라 Reflection 값 종류이므로 Engine 모듈에 둔다.

첫 Inspector는 다음 UI를 제공한다.

| Property | ImGui UI |
|---|---|
| `FIntProperty` | `DragInt` |
| `FBoolProperty` | `Checkbox` |
| `FFloatProperty` | `DragFloat` |
| `FDoubleProperty` | `DragScalar` |
| `FStringProperty` | `InputText` |
| `FNameProperty` | `InputText`, commit 시 `FName` 생성 |
| `FEnumProperty` | `Combo` |
| `FStructProperty` | 자식 프로퍼티 재귀 표시 |
| `FObjectProperty` | 객체 이름 표시, 초기에는 read-only |
| `FSoftObjectProperty` | Asset path 표시, 초기에는 read-only |
| `FArrayProperty` | 원소 재귀 표시, resize는 이후 구현 |

`FVector`, `FQuat`, `FTransform`은 Inspector의 구체 함수에서 특별 처리한다. `FVector`는 X/Y/Z를 한 행에 배치하고, `FTransform`은 Translation/Rotation/Scale 전용 행을 사용한다. `FQuat`은 권위 있는 저장 타입으로 유지하되 UI에서는 Pitch/Yaw/Roll `FRotator`로 변환해 편집하고, 변경된 값은 정규화한 `FQuat`으로 다시 반영한다. 그 밖의 `UScriptStruct`는 등록된 자식 프로퍼티를 재귀적으로 그린다.

고정 배열은 `FProperty::GetArrayDimension()`만큼 원소를 반복한다. 동적 `FArrayProperty`는 현재 `FArrayOps`가 private이므로 Inspector 편집을 추가할 때 `GetNum()`, `GetElementPtr()`와 `Resize()` 같은 검증된 public 연산을 `FArrayProperty`에 제공한다. Inspector가 `FArrayOps` 함수 포인터를 직접 다루지 않는다.

### 값 변경 경로

Inspector는 Property가 제공하는 값 연산을 사용한다.

```text
ImGui Widget에서 임시 값 편집
	↓ 값이 실제로 변경됨
FProperty::ContainerPtrToValuePtr(Object)
	↓
FProperty::CopyValue(Destination, EditedValue)
	↓
UObject::PostEditProperty(Property)
```

직접 `memcpy`하거나 프로퍼티의 C++ 타입을 추정해 대입하지 않는다. String, Struct, Object reference와 Array는 타입별 copy 의미가 필요하기 때문이다.

`ContainerPtrToValuePtr(void*)`는 Reflection의 public mutable 접근 API로 유지한다. `PostEditProperty()`도 Editor가 호출할 수 있는 `UObject` public virtual 함수로 제공한다. `FInspectorPanel`이나 별도 `FPropertyEditor`를 friend로 선언하지 않는다.

### PostEditProperty 전달 값

현재 구현은 최상위 멤버 프로퍼티를 전달한다.

```cpp
class UObject
{
public:
	virtual void PostEditProperty(const FProperty& Property) {}
};
```

기본 구현은 아무 작업도 하지 않는다. 첫 구현에서는 변경 이벤트 객체를 만들지 않고 최상위 `FProperty`만 전달한다.

### PostEditProperty 책임

Component override는 편집된 저장값에서 파생되는 상태만 갱신한다.

| Component | 예시 처리 |
|---|---|
| `UTransformComponent` | 회전 정규화, 자신과 자손의 렌더 Proxy Dirty 표시 |
| `UPrimitiveComponent` | MarkPrimitiveSceneProxy로 등록 Proxy 변경 표시 |
| `UMeshComponent` | Mesh 변경 시 Proxy Dirty 표시. Material은 향후 구현 |
| `UMovementComponent` | 현재 Tick에서 편집된 RotationRate를 읽음 |

`PostEditProperty()`에서 UI를 열거나 Selection을 변경하지 않는다. 다른 객체를 파괴하거나 Component 등록 상태를 바꿔야 한다면 Editor command 또는 안전한 재등록 queue로 넘긴다.

Component가 Setter를 갖지 않는다는 정책은 설정 프로퍼티마다 단순 대입 Setter를 만들지 않는다는 뜻이다. runtime gameplay action에 필요한 명령형 API까지 금지하지 않는다. 예를 들어 `AddForce()`나 `Teleport()`는 프로퍼티 Setter와 다른 동작 계약이다.

### ImGui 편집 commit

숫자 drag와 color 편집은 값이 바뀐 각 프레임에 복사와 `PostEditProperty()`를 호출해 Viewport에 live preview를 제공한다. Text는 임시 buffer에서 편집하고 Enter 또는 item deactivation 시 유효한 값만 commit한다.

Undo/Redo transaction과 변경 이력 저장은 현재 범위에 포함하지 않는다.

## FViewportPanel

Viewport Panel은 ImGui 창 안에 엔진 Render Target을 표시하고 실제 이미지 사각형을 입력 대상으로 등록한다.

```text
FViewportPanel
├─ ImGui Window "Viewport"
├─ FViewport
│  ├─ Render Target
│  ├─ Depth Target
│  └─ pixel size
└─ FLevelEditorViewportClient : FEditorViewportClient
   ├─ FEditorViewportClient : IInputTarget
   │  ├─ Camera state
   │  └─ input interpretation
   └─ Editor World 조회와 ViewFamily 구성
```

`FViewport`는 출력 surface와 크기를 관리한다. `FEditorViewportClient`는 연결된 Viewport가 출력 가능한지 판정하고 공통 에디터 카메라와 입력을 관리한다. 파생 client는 어떤 World나 Asset을 볼지 결정한다. 현재 `FLevelEditorViewportClient`는 기본 World 조회와 ViewFamily 구성을 그대로 사용한다.

`EditorViewportCamera.h/.cpp`에 Transform·ViewMode·Camera 선언과 Transform 함수 구현을 모은다. `FEditorViewportCamera`는 `FEditorViewportCameraTransform ViewTransform`, 에디터 전용 ViewMode와 이동 Sensitivity를 보관한다. 기준 이동 속도는 10cm/s이고 Sensitivity는 여기에 곱하는 배율이다. Sensitivity 기본값은 1.0이고 편집 범위는 0.1~20.0이다. 모든 뷰 모드는 ViewLocation/ViewRotation을 단일 기준으로 사용하며, 직교 방향을 선택하면 `OnCameraStateChanged()`가 해당 방향의 위치와 회전을 ViewTransform에 기록한다. UI는 `GetCamera()`로 상태를 직접 편집한 뒤 변경 통지를 호출한다. Viewport의 Camera popup은 ViewLocation을 XYZ cm, ViewRotation을 Pitch/Yaw/Roll degree 순서로 표시하고 편집 결과를 `OnViewTransformChanged()`로 통지한다. ViewTransform의 TranslateWorld/TranslateLocal은 cm, Rotate의 Yaw/Pitch는 degree를 사용한다. LookAt은 같은 위치를 무시하고, 수직 목표에서는 Yaw를 유지하며 Pitch는 정확한 목표 방향을 허용한다. 마우스 Rotate는 기존처럼 Pitch를 ±89도로 제한한다.

Camera.FOV는 사용자가 편집하는 수직 시야각 degree 값이며 기본값은 60도다. 투영 행렬을 만들 때만 radians로 변환한다. OrthoZoom은 직교 화면 **폭**이고 기본값은 10cm이다. 이전 높이 10cm 기준과 달리 직교 높이는 OrthoZoom / AspectRatio로 계산한다. AspectRatio는 투영 행렬 계산 시 실제 viewport 크기로 갱신하며, NearClip/FarClip 기본값은 0.1/10000cm이다. 현재 UCameraComponent/FCameraState는 구현되어 있지 않으므로 해당 타입과의 필드 동등성은 아직 계약하지 않는다.

Viewport 상단 toolbar의 `Camera` 버튼은 popup에서 WASDQE 이동과 휠 전후 이동 감도를 조절한다. Perspective fly camera는 우클릭을 누른 동안에만 동작하며, W/S는 시선 방향, A/D는 카메라 좌우, Q/E는 월드 아래/위로 이동한다. 우클릭 drag의 raw mouse delta는 Yaw와 Pitch를 바꾸며 Pitch는 뒤집힘을 막기 위해 -89도에서 89도로 제한한다. 카메라 basis는 row-vector 규약에 맞춰 local Pitch 회전 뒤 world Yaw 회전을 적용한다.

toolbar의 `View` 버튼은 원형 radio 항목으로 Perspective와 Top, Bottom, Left, Right, Front, Back 직교 뷰를 선택하는 popup을 연다. 현재는 모드별 카메라 상태를 별도로 보존하지 않는다. 직교 뷰 선택 시 같은 ViewTransform에 해당 방향의 위치·회전을 기록하며, WASD는 화면 평면을 이동하고 마우스 휠은 직교 표시 범위를 확대하거나 축소한다.

### Render Target

Panel content 영역의 framebuffer pixel 크기가 바뀌면 다음 렌더 전에 offscreen color/depth target을 재생성한다. 0 크기, collapse 상태와 최소화 상태에서는 렌더링하지 않는다.

ImGui에는 RHI native D3D11 SRV를 직접 노출하지 않는다. ImGui render backend가 `FTextureHandle`을 `ImTextureID`로 변환하거나 등록하는 API를 제공한다.

```text
SceneRenderer Render
	↓
Viewport Offscreen Texture
	↓ ImGui backend texture binding
ImGui::Image
	↓
Back Buffer의 Editor UI
```

World rendering은 ImGui draw data 제출 전에 수행한다. UI layout을 먼저 구성해야 이번 프레임 Viewport 크기를 알 수 있으므로 프레임은 UI build와 Viewport render를 분리한다.

### 프레임 Viewport 렌더 여부

`FViewportPanel::Draw()`는 숨김, collapse, 0 크기 상태에서 offscreen target을 해제한다. 유효한 Image 영역이면 target 크기를 맞춘 뒤 ImGui 이미지와 입력 영역을 등록한다.

`UEditorEngine`은 등록된 Client에서 `BuildSceneViewFamily()`를 호출한다. 유효한 Viewport 또는 World가 없으면 결과를 반환하지 않는다. 유효하면 `BuildSceneView()`로 시점을 만들고 Scene 참조, View 배열, 출력 타깃과 ShowFlags를 Family에 담는다.

`FEditorViewportClient::GetWorld()`는 기본적으로 `GEngine->GetWorld()`를 반환하고 `GetScene()`은 그 World의 Scene을 반환한다. 별도 프리뷰 월드를 사용하는 파생 Client는 GetWorld를 재정의한다. SceneRenderer는 World를 순회하지 않고 Scene의 Proxy만 읽는다.

```text
Panel UI 구성 → 타깃 크기와 입력 영역 확정
  → Client 카메라 Tick
  → BuildSceneViewFamily → BuildSceneView
  → 메인 스레드에서 SceneRenderer 실행
  → ImGui 합성
```

Client 목록은 메인 스레드에서 순회한다. SceneRenderer는 ViewFamily를 사용하며 Client나 Panel을 순회하지 않는다.

### Viewport 입력 등록

`ImGui::Image()` 직후 실제 item 상태로 target을 등록한다.

```cpp
InputRouter.RegisterTarget(
	ViewportClient,
	ImGui::IsItemHovered(),
	ImGui::IsWindowFocused());
```

Panel title bar, Camera Speed toolbar와 scrollbar hover는 Viewport image hover로 취급하지 않는다. 기즈모가 별도 `IInputTarget`이 되면 Viewport보다 뒤에 등록하여 겹친 영역에서 우선권을 얻는다.

오른쪽 마우스 카메라 회전은 `CaptureMouse()`, 버튼 해제는 `ReleaseMouse()`를 반환한다. 키보드 카메라 이동은 Viewport click에서 `SetKeyboardFocus()`를 요청한다.

## FConsolePanel

Console Panel은 Editor 로그 sink의 ring buffer를 표시한다.

```text
Engine Log
	↓ FEditorLogSink
bounded message buffer
	↓
FConsolePanel
```

첫 기능은 다음으로 제한한다.

- 로그 레벨별 색상
- 문자열 필터
- Clear
- Auto-scroll
- 최대 메시지 수 제한

Panel을 닫아도 sink는 로그를 계속 수집한다. Worker thread 로그가 들어오면 sink가 짧게 lock하고 UI 프레임 시작에 표시용 snapshot을 만든다. Console Panel이 logger 내부 container를 순회하지 않는다.

명령 입력은 command registry가 생긴 뒤 추가한다. 로그 출력과 command console을 처음부터 하나의 추상 시스템으로 묶지 않는다.

## 보류: FContentPanel

Content Panel은 현재 구현하지 않는다. 향후 프로젝트 Asset 탐색이 필요해질 때 다음 경계를 따른다.

```text
Project Content Root
	↓ Asset Registry 또는 초기 filesystem scan
Folder tree + Asset entries
	↓
FContentPanel
```

첫 기능은 다음과 같다.

- 폴더 탐색
- Asset 이름과 종류 표시
- 선택과 더블 클릭
- Viewport 또는 Inspector로 drag source 제공

Asset Registry가 아직 없으면 Content Panel 내부에서 최소 filesystem scan으로 시작한다. 이 코드가 import 상태, Asset type와 변경 감시를 다루기 시작하면 Engine Asset Registry로 옮긴다.

Object와 SoftObject 프로퍼티 picker는 Content Panel 클래스를 직접 참조하지 않는다. Asset path 또는 선택 결과를 drag-and-drop payload로 전달한다.

## 입력 라우팅

일반 ImGui Panel은 `IInputTarget`을 구현하지 않는다. Checkbox, InputText, TreeNode와 MenuItem은 ImGui가 직접 처리하며 `WantCaptureMouse`, `WantCaptureKeyboard`, `WantTextInput` 상태가 엔진 입력 누출을 막는다.

`FInputRouter`에 등록하는 대상은 다음과 같다.

- Viewport image
- Viewport 내부 기즈모
- 향후 PIE Game Viewport
- ImGui 바깥 동작이 필요한 별도 native editor interaction

프레임 입력 순서는 다음과 같다.

```text
UEditorEngine::ProcessInput
	↓
FInputRouter::BeginFrame(InputSnapshot)
	↓
FImGuiSystem::BeginFrame
	├─ ImGui::NewFrame
	└─ 첫 ImGui capture state 전달
		↓
FImGuiSystem::Draw
	├─ DockSpace와 MenuBar
	├─ 일반 Panel ImGui widget
	└─ Viewport image와 IInputTarget 등록
		↓
최종 ImGui capture state 전달
		↓
FInputRouter::RouteInput
		↓
World / ViewportClient Tick
		↓
FImGuiSystem::EndFrame
	└─ ImGui::Render
		↓
메인 스레드에서 ViewFamily offscreen render
		↓
ImGui Draw Data의 Back Buffer 합성과 제출
```

ImGui capture 상태는 `NewFrame()` 직후와 모든 Panel 구성 이후 두 번 누적한다. Text widget이 같은 프레임에 focus를 해제해도 해당 KeyDown/KeyUp sequence가 Viewport로 넘어가지 않는다.

## EditorEngine 프레임 조율

메인 스레드에서 UI 구성 → 입력 라우팅 → World Tick → Client 카메라 Tick → ViewFamily 구성 → SceneRenderer 실행 → ImGui 합성 → Submit/Present 순서로 수행한다.

각 World는 Viewport 개수와 무관하게 한 번 Tick한다. Component의 변경은 Proxy 자체에 Dirty로 표시되고 World.Tick 마지막에 지속적인 Scene Proxy에 반영되며 전체 Snapshot은 만들지 않는다. EditorEngine이 Family마다 FSceneRenderer를 지역 객체로 생성하고 Render(Renderer)를 호출한다. FSceneRenderer는 View별 컬링과 불투명 패스 정렬을 수행한다.

현재는 전용 Render Thread와 ImGui Context mutex가 없다. SceneRenderer는 Proxy 갱신을 수행하지 않고 GetProxies()로 확정된 상태를 읽는다. OpaqueCommands는 `FOpaquePass::AddPass()`가 만드는 임시 Node 데이터이며 프레임 간 캐시는 없다.

Render Thread 분리와 Render Graph의 자원 추적은 향후 목표다. UI 구성은 메인 스레드에 유지하고, 비동기 렌더링에는 ViewFamily·타깃·ImGui draw data의 수명 보장이 필요하다. 다중 렌더 worker는 현재 목표 범위에 포함하지 않는다. 구체 경계는 [Rendering-Architecture.md](Rendering-Architecture.md)를 따른다.

Native 출력이 0 크기면 GPU 프레임을 생략한다. ViewFamily가 없어도 창 출력이 유효하면 UI 합성과 Present는 수행한다.

## Inspector 변경과 Runtime 반영

프로퍼티 종류에 따라 반영 시점이 다르다.

| 변경 | 처리 방식 |
|---|---|
| 단순 gameplay 수치 | 값 복사 후 다음 Tick에서 읽음 |
| Transform | MarkPrimitiveSceneProxy로 표시한 World Transform과 Proxy 상태를 World.Tick 끝에서 반영 |
| Mesh / Visibility | MarkPrimitiveSceneProxy로 표시하고 World.Tick 끝에서 반영. Material 갱신은 향후 구현 |
| Physics shape | Physics state 재생성 예약 |
| Component enable 설정 | 안전한 등록/활성 상태 전이 함수 호출 |

`PostEditProperty()` 안에서 Render 또는 Physics container를 순회 중에 즉시 제거하지 않는다. 해당 subsystem이 안전 지점에서 처리할 pending update를 기록한다.

프로퍼티 직접 쓰기는 상태 불변식을 우회할 수 있으므로 생명주기 제어 값은 `NoEdit`으로 표시한다.

- Owner
- Registered 상태
- BegunPlay 상태
- Active 내부 상태
- 실행 node handle
- Render/Physics native handle

사용자가 편집할 설정값과 엔진이 관리하는 runtime 상태를 UPROPERTY flag로 분리한다.

## Undo와 Redo

Undo/Redo는 현재 구현하지 않는다. 향후 도입할 때 Inspector 변경 경로의 확장을 검토한다.

향후 transaction은 다음 데이터를 소유한다.

```text
Object identity
MemberProperty identity
Array index 또는 nested path
Before value copy
After value copy
```

값 저장은 Property의 타입별 수명과 copy 연산을 사용한다. raw byte snapshot은 String, Array와 Object reference에 사용할 수 없다.

Interactive drag 한 번은 transaction 하나다. drag 중에는 live preview를 위해 여러 번 `PostEditProperty()`를 호출하되 최종 Undo 기록은 시작 값과 종료 값 하나씩만 보관한다.

## 저장과 Layout

ImGui layout은 기존 `FPaths::ImGuiSettingsPath()`에 저장한다. Panel visibility처럼 프로젝트와 무관한 사용자 UI 상태도 초기에는 같은 ini를 사용한다.

World, Node와 Component 프로퍼티 저장은 ImGui ini와 분리한다. Inspector가 값을 바꾼 경우 Editor World 또는 Asset의 dirty 상태를 표시하고 명시적인 Save command가 Reflection serialization 경로를 사용한다.

## 현재 구현 상태

### 구현됨

- ImGui context와 Win32/RHI backend 초기화
- ImGui ini 경로 생성과 저장
- `FImGuiSystem`의 BeginFrame, Draw, Render와 Shutdown
- `FInputRouter`의 frame snapshot 보관
- ImGui capture 상태 누적
- hovered, keyboard focus와 mouse capture target 라우팅
- key/button Down-Up sequence owner 보존
- Editor WorldContext 생성과 Context ID 조회
- 기본 Reflection 프로퍼티 열거와 metadata
- Numeric, String, Name, Enum, Struct, Object, SoftObject와 Array Property
- ImGui Docking 활성화와 root DockSpace
- Hierarchy, Inspector, Viewport, Console Panel과 Main MenuBar
- Node 단위 Editor Selection과 World/Level/Node tree
- Reflection property kind와 Inspector scalar/struct 편집
- `UObject::PostEditProperty(const FProperty&)`
- Viewport offscreen color/depth target과 ImGui texture 표시
- `FEditorViewportClient` Tick 및 BuildSceneView/BuildSceneViewFamily
- 메인 스레드에서 SceneRenderer와 ImGui 합성 실행
- `FLevelEditorViewportClient` 입력 등록
- Editor log sink와 Console 출력

### 미구현

- Render Thread 분리와 Render Graph 자원 추적
- 기즈모
- Content 탐색과 Asset Registry 연결
- Component 단위 선택
- Undo/Redo와 Editor command
- Array, Object 및 SoftObject 프로퍼티 편집

## 관련 문서

- [Reflection-Architecture.md](Reflection-Architecture.md)
- [Input-Architecture.md](Input-Architecture.md)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [World-Architecture.md](World-Architecture.md)
- [World-Architecture-Roadmap.md](World-Architecture-Roadmap.md)
- [Conventions.md](Conventions.md)

## 관련 파일

- [EditorEngine.h](../KnotEngine/Source/Editor/Runtime/EditorEngine.h)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/Runtime/EditorEngine.cpp)
- [ImGuiSystem.h](../KnotEngine/Source/Editor/ImGui/ImGuiSystem.h)
- [ImGuiSystem.cpp](../KnotEngine/Source/Editor/ImGui/ImGuiSystem.cpp)
- [InputRouter.h](../KnotEngine/Source/Editor/Input/InputRouter.h)
- [InputRouter.cpp](../KnotEngine/Source/Editor/Input/InputRouter.cpp)
- [Viewport.h](../KnotEngine/Source/Editor/Viewport/Viewport.h)
- [Viewport.cpp](../KnotEngine/Source/Editor/Viewport/Viewport.cpp)
- [EditorViewportCamera.h](../KnotEngine/Source/Editor/Viewport/EditorViewportCamera.h)
- [EditorViewportClient.h](../KnotEngine/Source/Editor/Viewport/EditorViewportClient.h)
- [LevelEditorViewportClient.h](../KnotEngine/Source/Editor/Viewport/Level/LevelEditorViewportClient.h)
- [Object.h](../KnotEngine/Source/Engine/Object/Object.h)
- [Class.h](../KnotEngine/Source/Engine/Object/Class.h)
- [Property.h](../KnotEngine/Source/Engine/Object/Property.h)
