# InputRouter — 애플리케이션 및 뷰포트 계층 라우팅

## 계층 구조

```
FInputRouter (전역 라우터)
├── UI Layer (ImGui / 자체 GUI — 입력 소비 최우선 판별)
├── UGameViewportClient
└── FViewportClient
    └── FEditorViewportClient
        ├── FLevelEditorViewportClient
        └── FAssetEditorViewportClient
            └── FMeshEditorViewportClient
                ├── FStaticMeshEditorViewportClient
                └── FSkeletalMeshEditorwViewportClient
            ├── FMaterialEditorViewportClient
            └── FAnimationEditorViewportClient
            └── FPhysicsEditorViewportClient
```

---

## 입력 처리

### 1. Input Snapshot

`InputSystem::MakeSnapshot()`을 호출하여 `FInputSystemSnapshot`을 생성한다.  
스냅샷은 두 가지 정보를 모두 포함한다.

- **State**: 현재 프레임의 키/버튼 상태 (Pressed / Released / Held)
- **EventQueue**: 프레임 간 발생한 입력 이벤트의 순서 보존 큐 (프레임 드롭 시 이벤트 유실 방지)

---

### 2. FInputRouter — 전역 라우팅

`FInputRouter`가 스냅샷을 수신하여 아래 우선순위 순서대로 라우팅을 수행한다.

1. **UI Layer 소비 판별**: 오버레이된 ImGui 또는 자체 GUI에 입력을 먼저 전달한다. UI가 입력을 소비(Consume)한 경우 이후 단계를 진행하지 않는다.
2. **CapturedViewport 우선**: 마우스 캡처 또는 키 포커스를 획득한 ViewportClient가 있으면 해당 클라이언트에 스냅샷을 전송한다.
3. **HoveredViewport 판별**: 캡처된 뷰포트가 없으면 현재 마우스 커서가 위치한 ViewportClient를 검사하여 전송한다.
4. **PIE 모드 전환**: PIE(Play In Editor)가 활성화되면 입력을 우회 전송하는 대신, 포커스 대상 자체를 `UGameViewportClient`로 스위칭하여 이후 라우팅이 동일한 경로로 처리되도록 한다. PIE 종료 시 포커스를 이전 EditorViewportClient로 복원한다.

> **설계 원칙**: FInputRouter는 "누구에게 보낼지"만 결정한다. 입력의 해석과 반응은 각 ViewportClient와 그 하위 Controller의 책임이다.

---

### 3. ViewportClient — Controller 위임 및 Provider DI

각 ViewportClient는 입력 스냅샷을 수신하면 자신에게 연결된 Controller에 전달한다.

#### Controller 대응 관계

| ViewportClient | Controller |
|---|---|
| `UGameViewportClient` | `APlayerController` (월드 내 액터) |
| `FLevelEditorViewportClient` | `FLevelEditorController` |
| `FStaticMeshEditorViewportClient` | `FStaticMeshEditorController` |
| `FSkeletalMeshEditorViewportClient` | `FSkeletalMeshEditorController` |
| `FMaterialEditorViewportClient` | `FMaterialEditorController` |
| `FAnimatioNEditorViewportClient` | `FAnimationEditorController` |
| `FMaterialEditorViewportClient` | `FMaterialEditorController` |

`APlayerController`는 월드 내 액터로서 UE 게임플레이 프레임워크의 수명 주기를 따른다.  
나머지 Editor Controller는 월드 외부의 순수 C++ 객체로, ViewportClient와 동일한 수명 주기를 가진다.

#### Provider 기반 Dependency Injection

Controller는 ViewportClient로부터 컨텍스트 정보를 얻기 위해 단일 `IViewportContext` 대신, 기능별로 분리된 Provider 인터페이스를 주입받는다. ViewportClient는 자신이 지원하는 Provider 인터페이스를 구현하며, Controller는 초기화 시점에 필요한 Provider 포인터만 주입받는다. 지원하지 않는 Provider는 `nullptr`로 처리한다.

**Provider 인터페이스 정의**

```cpp
struct ICameraProvider  { virtual FEditorCamera* GetCamera()    = 0; };
struct IWorldProvider   { virtual UWorld*         GetWorld()     = 0; };
struct IGizmoProvider   { virtual FGizmo*          GetGizmo()     = 0; };
struct ISelectionProvider { virtual FSelection*   GetSelection() = 0; };
```

**Controller별 주입 Provider**

| Controller | 주입받는 Provider |
|---|---|
| `FLevelEditorController` | `ICameraProvider`, `IWorldProvider`, `IGizmoProvider`, `ISelectionProvider` |
| `FStaticMeshEditorController` | `ICameraProvider`, `IGizmoProvider` |
| `FSkeletalMeshEdtiorController` | `ICameraProvider`, `IGizmoProvider` |
| `FAnimationEditorController` | `ICameraProvider` |
| `FMaterialEditorController` | `ICameraProvider` |

**초기화 예시**

```cpp
// FLevelEditorViewportClient 생성 시
Controller = MakeUnique<FLevelEditorController>(
    /*Camera*/    this,   // ICameraProvider 구현체
    /*World*/     this,   // IWorldProvider  구현체
    /*Gizmo*/     this,   // IGizmoProvider  구현체
    /*Selection*/ this    // ISelectionProvider 구현체
);
```

---

### 4. EnhancedInputManager — 중앙 집중형 서브시스템

#### 소유권

`EnhancedInputManager`는 엔진 서브시스템 레벨에 단일 인스턴스로 존재한다.  
개별 Controller는 Manager를 소유하지 않는다.

#### Context 등록/해제 생명주기

Controller는 포커스 획득·상실 시점에만 Manager와 상호작용한다.

```
Controller::OnActivated()
  → Manager.AddContext(MyMappingContext, Priority)
  → Manager.BindActions(MyActionBindings)   ← 바인딩도 이 시점에 등록

Controller::OnDeactivated()
  → Manager.RemoveBindings(MyActionBindings)
  → Manager.RemoveContext(MyMappingContext)
```

> **바인딩과 컨텍스트 등록의 분리**: `AddContext`는 "어떤 키가 어떤 Action을 발동하는가"를 Manager에 알린다. `BindAction`은 "Action이 발동되면 어떤 함수를 호출하는가"를 등록한다. 두 작업은 순서상 `AddContext` → `BindAction`이어야 한다.

#### InputMappingContext 소유권

IMC 인스턴스는 각 Controller가 멤버로 보유한다. Controller는 Manager에 IMC의 포인터/참조를 전달하여 등록하며, Manager는 IMC를 복사하지 않는다. Controller가 소멸될 때 `RemoveContext`가 먼저 호출된 후 IMC가 해제된다.

```cpp
class FLevelEditorController
{
    FInputMappingContext EditorMappingContext;  // Controller가 소유
    TArray<FInputBindingHandle> Bindings; // 등록된 바인딩 핸들 보관

    void OnActivated(FEnhancedInputManager& Manager)
    {
        Manager.AddContext(EditorMappingContext, Priority=0);
        Bindings.Add(Manager.BindAction(ActionEditorMove,
            ETriggerEvent::Triggered,
            [this](const FInputActionValue& V){ HandleMoveCamera(V); }));
    }

    void OnDeactivated(FEnhancedInputManager& Manager)
    {
        for (auto& Handle : Bindings)
            Manager.RemoveBinding(Handle);
        Bindings.Reset();
        Manager.RemoveContext(EditorMappingContext);
    }
};
```

#### Asset Editor Controller의 MappingContext 조합 구조

Asset Editor 4종은 공통 동작(카메라 오빗, 줌, 패닝)과 전용 동작(본 선택, 머티리얼 파라미터 조작 등)을 MappingContext 조합으로 구현한다.

**우선순위 규칙**

| MappingContext 멤버 변수 | Priority | 등록 주체 |
|---|---|---|
| `PreviewCommonMappingContext` (오빗, 줌, 패닝) | 0 (낮음) | `FPreviewController::OnActivated()` |
| `SkeletalMeshMappingContext` (본 선택 등) | 1 (높음) | `FSkeletalMeshEditorController::OnActivated()` |
| `MaterialMappingContext` (파라미터 조작 등) | 1 (높음) | `FMaterialEditorController::OnActivated()` |

- 전용 Context의 Priority가 공통 Context보다 높다. 동일 키에 대해 전용 Action이 공통 Action을 덮어쓸 수 있다.
- 충돌이 없는 키는 두 Context가 동시에 활성화된 상태로 병존한다.
- `FAssetEditorController::OnActivated()`는 반드시 `Super::OnActivated()`를 먼저 호출하여 공통 Context가 낮은 우선순위로 먼저 등록되도록 보장한다.

**활성화 순서 예시 — FSkeletalMeshEditorController**

```
FAssetEditorController::OnActivated()
  → Manager.AddContext(PreviewCommonMappingContext, Priority=0)
  → BindCommonActions()

FSkeletalMeshEditorController::OnActivated()  [Super 호출 후]
  → Manager.AddContext(SkeletalMeshMappingContext, Priority=1)
  → BindSkeletalActions()
```

---

## 용어 정리

| 용어 | 정의 |
|---|---|
| `ApplicationInputRouter` | 전역 입력 라우터. UI 소비 판별 후 ViewportClient로 스냅샷을 전달한다. |
| `FInputSystemSnapshot` | 단일 프레임의 입력 State와 EventQueue를 포함하는 불변 스냅샷 객체. |
| `ViewportClient` | 렌더 뷰포트와 1:1 대응하는 객체. 입력 수신 후 Controller에 위임한다. |
| `Controller` (Editor) | 월드 외부의 순수 C++ 입력 처리 객체. UE `APlayerController`와 무관하다. |
| `Provider` | ViewportClient가 구현하는 컨텍스트 공급 인터페이스 (`ICameraProvider` 등). |
| `EnhancedInputManager` | 엔진 서브시스템 레벨의 단일 입력 처리 인스턴스. Context와 Binding을 관리한다. |
| `InputMappingContext` | Action-Key-Trigger-Modifier 매핑 데이터. Controller가 멤버 변수로 소유하고 Manager에 등록한다. 변수명은 `EditorMappingContext`, `PreviewCommonMappingContext` 등 역할을 명시하는 서술형을 사용한다. |
| `CapturedViewport` | 마우스 캡처 또는 키 포커스를 보유한 ViewportClient. |
| `HoveredViewport` | 마우스 커서가 위치한 ViewportClient. |
