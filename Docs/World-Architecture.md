# Knot Engine World Architecture

## 문서 목적

이 문서는 현재 구현된 Knot Engine의 World 식별 방식, `UWorld`-`ULevel`-`UNode`-`UComponent` 소유 관계, 플레이 생명주기와 프레임 실행 경로를 설명한다.

등록 기반 Tick과 여섯 실행 Phase를 포함한 향후 구조는 [World-Architecture-Roadmap.md](World-Architecture-Roadmap.md)에서 별도로 다룬다.

## 현재 설계 원칙

- Engine은 전역 Current World 포인터 대신 `FWorldContext` 배열로 World를 식별한다.
- World, Level, Node와 Component는 명시적인 소유 계층을 이룬다.
- Transform 공간 계층은 객체 소유 계층과 별개다.
- Level은 Node를 공간 부모 관계와 무관한 평탄한 배열로 소유한다.
- Node는 정확히 하나의 TransformComponent를 가지며 나머지 기능은 Component 합성으로 구성한다.
- 게임별 Actor 파생 계층을 만들지 않고 `UNode`를 Component 조합으로 확장한다.
- 플레이 생명주기와 Tick은 현재 소유 계층을 직접 순회하며 전달한다.
- Editor는 WorldContext ID로 렌더링할 World를 선택한다.

## 전체 구조

```text
UEngine
└─ FWorldContext[]
   └─ UWorld
      ├─ PersistentLevel ─┐
      └─ ULevel[] <───────┘
         └─ UNode[]
            ├─ UTransformComponent  정확히 하나
            └─ UComponent[]         Transform을 포함한 합성 기능
```

```text
소유 계층
UWorld → ULevel → UNode → UComponent

공간 계층
UTransformComponent Parent → Child TransformComponents
```

Node의 Transform 부모를 바꿔도 Level의 `Nodes` 배열 소속은 바뀌지 않는다. 같은 Node가 두 Component를 소유한다는 사실도 Component 사이의 실행 순서를 정의하지 않는다.

## 현재 디렉터리

```text
KnotEngine/Source/Engine/
├─ Runtime/
│  ├─ Engine.h/.cpp
│  └─ EngineLoop.h/.cpp
├─ World/
│  ├─ WorldContext.h
│  ├─ World.h/.cpp
│  ├─ Level.h/.cpp
│  └─ Node.h/.cpp
└─ Component/
   ├─ Component.h/.cpp
   ├─ TransformComponent.h/.cpp
   ├─ RendererComponent.h/.cpp
   ├─ MeshRendererComponent.h/.cpp
   └─ MovementComponent.h/.cpp
```

| 계층 | 현재 책임 |
|---|---|
| `UEngine` | WorldContext 생성, 조회, 파괴와 World 참조 수집 |
| `FWorldContext` | Context ID, World 종류와 World 참조 보관 |
| `UWorld` | Level 수명, PersistentLevel과 플레이 상태 관리 |
| `ULevel` | Node의 평탄한 소유와 생명주기 전달 |
| `UNode` | 필수 Transform과 나머지 Component 소유 |
| `UComponent` | Owner를 통한 World 접근과 공통 플레이 상태 제공 |
| `UTransformComponent` | local transform과 Transform 부모·자식 관계 관리 |

## FWorldContext

`FWorldContext`는 Engine 내부에서 World 하나를 식별하는 값 타입이다. Editor, Game과 PIE처럼 서로 다른 용도의 World를 같은 Engine 안에서 구분한다.

```cpp
struct FWorldContext
{
	uint64 ContextId = 0;
	EWorldType WorldType = EWorldType::Editor;
	TObjectPtr<UWorld> World = nullptr;
};
```

`UEngine::CreateWorldContext()`는 Context와 World를 함께 생성한다. `DestroyWorldContext()`는 Context를 배열에서 제거한 뒤 World를 파괴한다.

외부 객체는 배열 원소의 주소를 장기간 보관하지 않는다. Context ID를 저장하고 `FindWorld()`로 World를 다시 조회하여 배열 재할당에 따른 참조 무효화를 피한다.

`FWorldContext`는 UObject가 아니므로 `UEngine::AddReferencedObjects()`가 각 Context의 World를 Reference Collector에 명시적으로 전달한다.

```text
UEditorEngine
├─ Editor ContextId → Editor World
└─ PIE ContextId    → PIE World
```

현재 EditorEngine은 Editor Context ID를 보관하고 해당 ID로 World를 조회한다. 여러 Viewport가 같은 Context를 표시하더라도 World 시뮬레이션은 Context 단위로 수행한다.

## UWorld

`UWorld`는 하나의 독립적인 시뮬레이션을 나타내는 최상위 객체다. 현재는 Level 수명과 플레이 상태를 관리하고 Tick 및 Render 요청을 모든 Level에 전달한다.

World 생성 시 PersistentLevel을 함께 생성한다. PersistentLevel은 `Levels` 배열에도 포함되며 일반 `RemoveLevel()`로 제거할 수 없다.

```text
UWorld
├─ PersistentLevel ─┐
└─ Levels[] <───────┘
```

추가 Level은 `CreateLevel()`로 생성한다. `RemoveLevel()`은 대상 Level의 플레이를 먼저 종료하고 배열에서 제거한 뒤 객체를 파괴한다.

현재 플레이 상태는 다음 세 가지다.

| 상태 | 의미 | Component Tick |
|---|---|---|
| `Stopped` | 편집 또는 플레이 종료 상태 | 실행하지 않음 |
| `Playing` | 게임플레이 진행 상태 | 실행 |
| `Paused` | 게임플레이 일시 정지 상태 | 실행하지 않음 |

`BeginPlay()`는 상태가 `Stopped`일 때만 World를 `Playing`으로 바꾸고 모든 Level에 전달한다. `EndPlay()`는 World를 `Stopped`로 바꾸고 모든 Level에 전달한다. Pause와 Resume은 상태만 전환하며 BeginPlay와 EndPlay를 다시 호출하지 않는다.

## ULevel

`ULevel`은 World에 속한 Node의 저장 및 관리 단위다. 생성 시 받은 World를 `OwningWorld`로 참조하고 소속 Node를 `Nodes` 배열에 평탄하게 소유한다.

Level은 Transform 공간 계층의 루트가 아니다. 서로 부모·자식인 Node도 Level 배열에는 각각 독립된 원소로 들어간다.

```text
ULevel::Nodes
├─ Node A
├─ Node B  Transform parent: A
└─ Node C  Transform parent: B
```

`CreateNode()`로 만든 Node는 즉시 배열에 들어간다. World가 `Stopped` 상태가 아니면 새 Node도 곧바로 BeginPlay를 받는다.

현재 Level은 다음 요청을 모든 Node에 전달한다.

- `BeginPlay()`
- `EndPlay()`
- `Tick()`
- `Render()`

## UNode와 Component 합성

`UNode`는 Level에 배치되는 최소 객체이자 Component 합성 단위다. `UNode`는 `final`이며 게임 기능을 Node 상속으로 확장하지 않는다.

모든 Node는 생성과 함께 정확히 하나의 `UTransformComponent`를 만든다. Transform은 빠른 접근을 위한 `Transform` 멤버와 소유를 나타내는 `Components` 배열 양쪽에서 참조한다.

```text
Cube UNode
├─ UTransformComponent
├─ UMeshRendererComponent
└─ UMovementComponent
```

`AddComponent<UTransformComponent>()`는 컴파일 시 금지된다. Transform 제거 API도 제공하지 않으므로 공개된 Node에는 항상 하나의 Transform이 존재한다.

일반 Component는 Owner Node를 통해 Level과 World에 접근한다.

```text
UComponent::GetWorld
    ↓
UNode::GetWorld
    ↓
ULevel::GetWorld
```

Node에 붙은 Component는 Owner가 설정된 뒤 즉시 등록된다. Component는 `Components` 배열 순서로 BeginPlay와 Tick을 받고, Node 파괴 시 등록을 해제한 뒤 역순으로 파괴된다. Transform이 가장 먼저 생성되므로 다른 Component보다 나중에 파괴된다.

## UComponent

`UComponent`는 공간 정보와 렌더링 상태를 직접 갖지 않는 공통 기반이다. Owner Node, World와 필수 Transform에 접근할 수 있다.

현재 공통 상태는 다음과 같다.

| 상태 | 의미 |
|---|---|
| Owned | `Owner != nullptr`에서 파생되는 Node 소유 상태 |
| `bIsRegistered` | Component가 Owner의 World에 등록된 상태 |
| `bHasBegunPlay` | 현재 World의 플레이 생명주기에 진입한 상태 |
| `bIsActive` | 플레이 중 Component 기능이 활성화된 상태 |
| `bTickEnabled` | 등록된 Component의 `TickComponent()` 실행 허용 상태 |

### 상태 변수

#### Owner와 Owned

`Owner`는 Component를 소유하는 Node의 `TObjectPtr`이다. Owned 상태를 별도 bool로 저장하지 않고 `Owner != nullptr`에서 파생한다.

```cpp
bool IsOwned() const
{
	return Owner != nullptr;
}
```

Owner가 있다는 사실만으로 Registered, BegunPlay 또는 Active인 것은 아니다. `UnregisterComponent()`도 소유권을 제거하지 않으므로 등록 해제 후에도 Owner는 유지된다. Owner는 Node가 Component를 소유 목록에서 제거하고 파괴하는 과정에서만 수명이 끝난다.

#### bIsRegistered

`bIsRegistered`는 Component가 Owner의 World에 연결되어 Runtime 기능을 등록할 수 있는 상태를 나타낸다.

```text
false → RegisterComponent() → true
true  → UnregisterComponent() → false
```

`RegisterComponent()`는 Owner가 있고 Registered, BegunPlay와 Active가 모두 false인 상태에서만 호출할 수 있다. 함수는 `bIsRegistered`를 먼저 true로 바꾼 뒤 `OnRegister()`를 호출하므로 파생 훅에서 `IsRegistered()`는 true다.

`UnregisterComponent()`는 BegunPlay 상태라면 먼저 `EndPlay()`를 호출한다. Active가 해제된 것을 확인하고 `OnUnregister()`를 호출한 뒤 `bIsRegistered`를 false로 바꾼다. 따라서 `OnUnregister()`가 실행되는 동안에도 Owner와 World에 접근할 수 있고 `IsRegistered()`는 true다.

현재 이 상태는 생명주기 순서를 고정하지만 Render, Physics와 Tick Registry에는 아직 연결되지 않는다. 해당 subsystem 연결은 `OnRegister()`와 `OnUnregister()`에서 추가한다.

#### bHasBegunPlay

`bHasBegunPlay`는 현재 World의 플레이 생명주기에 진입했는지를 나타낸다. Active와 달리 Component 기능을 일시적으로 끄더라도 유지된다.

```text
Registered
    ↓ BeginPlay
BegunPlay + Active
    ↓ Deactivate
BegunPlay + Inactive
    ↓ EndPlay
Registered
```

`BeginPlay()`는 `bHasBegunPlay`를 먼저 true로 바꾸고 `bAutoActivate`가 true일 때 `Activate()`를 호출한다. `EndPlay()`는 먼저 `Deactivate()`하여 파생 activation 정리를 끝낸 뒤 `bHasBegunPlay`를 false로 바꾼다. 따라서 `OnActivated()`와 `OnDeactivated()` 안에서 `HasBegunPlay()`는 true다.

이 상태는 BeginPlay 중복 호출을 막고, 수동으로 Deactivate된 Component에도 EndPlay를 정확히 한 번 전달하는 데 사용한다. Node는 Active 여부가 아니라 `HasBegunPlay()`로 BeginPlay와 EndPlay 전달 여부를 판단한다.

#### bIsActive

`bIsActive`는 BegunPlay 상태 안에서 Component 기능이 현재 활성화되어 있는지를 나타낸다.

```text
BegunPlay + Inactive
    ↓ Activate
BegunPlay + Active
    ↓ Deactivate
BegunPlay + Inactive
```

`Activate()`는 Registered와 BegunPlay 상태를 요구한다. 상태를 먼저 true로 바꾼 뒤 `OnActivated()`를 호출한다. 이미 Active이면 아무 작업도 하지 않는다.

`Deactivate()`는 상태를 먼저 false로 바꾼 뒤 `OnDeactivated()`를 호출한다. 이미 Inactive이면 아무 작업도 하지 않는다. Deactivate는 EndPlay를 뜻하지 않으므로 `bHasBegunPlay`와 `bIsRegistered`는 유지된다.

#### bTickEnabled

`bTickEnabled`는 Component가 Tick callback을 제공하는지 나타내는 capability가 아니라 현재 `TickComponent()` 호출을 허용하는 설정이다. 이 값을 변경해도 Registered, BegunPlay와 Active 상태는 바뀌지 않는다.

현재 직접 순회 Tick의 실행 조건은 다음과 같다.

```text
World.PlayState == Playing
&& Component.IsActive()
&& Component.IsTickEnabled()
```

향후 `FWorldExecutionManager`가 추가되면 Tick Registered는 별도 bool로 저장하지 않고 Manager 등록 상태에서 파생한다. 이때도 `bTickEnabled`는 등록 여부와 독립적인 실행 설정으로 유지한다.

### 상태 불변식

Component 상태는 다음 관계를 항상 만족해야 한다.

```text
BegunPlay → Registered → Owned
Active → BegunPlay
!Registered → !BegunPlay && !Active
```

`bTickEnabled`는 이 관계에 포함되지 않는다. Tick을 비활성화한 Component도 Registered, BegunPlay와 Active 상태를 유지할 수 있다.

`RegisterComponent()`와 `UnregisterComponent()`는 공통 상태 전이를 고정하는 non-virtual 함수다. 파생 Component는 `OnRegister()`와 `OnUnregister()`에서 subsystem 연결과 정리를 구현한다.

`BeginPlay()`는 `bHasBegunPlay`를 설정한 뒤 `bAutoActivate`가 true이면 `Activate()`를 호출한다. `EndPlay()`는 `Deactivate()`를 호출한 뒤 플레이 상태를 해제한다. 파생 Component가 BeginPlay와 EndPlay를 재정의하면 기반 구현을 호출해야 한다.

`Activate()`와 `Deactivate()`도 상태 전이를 고정하는 non-virtual 함수다. 중복 호출은 아무 작업도 하지 않으며 실제 파생 동작은 `OnActivated()`와 `OnDeactivated()`에서 구현한다. 현재 `Activate()`는 Registered이면서 BegunPlay인 Component에만 허용된다.

```text
Owned
    ↓ RegisterComponent
Registered
    ↓ BeginPlay
BegunPlay + Active
    ↕ Activate / Deactivate
BegunPlay + Inactive
    ↓ EndPlay
Registered
    ↓ UnregisterComponent
Owned
```

현재 Register 훅은 마련되어 있지만 Render, Physics와 Tick Registry에는 아직 연결되지 않는다.

## 현재 플레이 생명주기

플레이 생명주기는 소유 계층을 따라 전달된다.

```text
UWorld::BeginPlay
    ↓
ULevel::BeginPlay
    ↓
UNode::BeginPlay
    ↓
UComponent::BeginPlay
```

```text
UWorld::EndPlay
    ↓
ULevel::EndPlay
    ↓
UNode::EndPlay
    ↓
UComponent::EndPlay
```

World가 `Stopped`가 아닌 동안 생성한 Level, Node와 Component는 생성 또는 부착 시점에 BeginPlay를 받는다. Node 파괴 시 EndPlay를 먼저 전달한 뒤 Component를 역순으로 파괴한다.

Component 부착과 파괴의 전체 순서는 다음과 같다.

```text
AttachComponent
    ├─ Owner 설정
    ├─ Components 배열에 추가
    ├─ RegisterComponent → OnRegister
    └─ World가 플레이 중이면 BeginPlay
       └─ bAutoActivate이면 Activate → OnActivated

Node 파괴
    ├─ EndPlay → Deactivate → OnDeactivated
    ├─ UnregisterComponent → OnUnregister
    └─ Component 역순 파괴
```

## 현재 프레임 실행 순서

`FEngineLoop`는 입력 상태를 갱신한 뒤 Engine Tick을 한 번 호출한다. EditorEngine은 Context ID로 Editor World를 찾고 World의 Tick과 Render를 호출한다.

```text
FEngineLoop::Run
    ↓
UEditorEngine::ProcessInput
    ↓
UEditorEngine::Tick
    ├─ FindWorld(EditorContextId)
    ├─ ImGui Frame 구성과 입력 라우팅
    ├─ UWorld::Tick
    ├─ UWorld::Render
    └─ ImGui Draw 및 Present
```

현재 World Tick은 소유 배열을 직접 순회한다.

```text
UWorld::Tick
    ↓ 모든 Level
ULevel::Tick
    ↓ 모든 Node
UNode::Tick
    ↓ 모든 Component
UComponent::TickComponent
```

World가 `Playing`일 때만 Level 순회를 시작한다. Node는 Component가 Active이고 Tick Enabled일 때 `TickComponent(DeltaTime)`을 호출한다.

현재 실행 순서는 Level, Node와 Component 배열 순서의 결과다. 코드가 이 순서를 사용하고 있지만 명시적인 dependency 계약은 아니다.

## 현재 렌더 전달

렌더링도 현재 소유 계층을 직접 순회한다.

```text
UWorld::Render
    ↓
ULevel::Render
    ↓
UNode::Render
    ↓ RendererComponent
URendererComponent::Render
```

향후 Scene proxy 기반 렌더 구조는 [Rendering-Architecture.md](Rendering-Architecture.md)에서 다룬다.

## 현재 구현의 경계

현재 구조는 작은 장면과 단일 스레드 실행에는 충분하지만 다음 기능은 아직 제공하지 않는다.

- Register 훅과 Render, Physics 및 Tick Registry의 실제 연결
- Level visibility와 load 상태에 따른 Component 일괄 재등록
- 실행 Phase와 명시적인 Tick dependency
- 실행 중 등록 및 해제의 안전 지점
- Level, Node와 Component 지연 파괴
- Tick interval과 Paused Tick
- Task Graph와 worker thread 실행
- Scene proxy 기반 렌더 등록

이 기능들의 목표와 구현 순서는 [World-Architecture-Roadmap.md](World-Architecture-Roadmap.md)에 정의한다.

## 관련 파일

- [EngineLoop.h](../KnotEngine/Source/Engine/Runtime/EngineLoop.h)
- [EngineLoop.cpp](../KnotEngine/Source/Engine/Runtime/EngineLoop.cpp)
- [Engine.h](../KnotEngine/Source/Engine/Runtime/Engine.h)
- [Engine.cpp](../KnotEngine/Source/Engine/Runtime/Engine.cpp)
- [WorldContext.h](../KnotEngine/Source/Engine/World/WorldContext.h)
- [World.h](../KnotEngine/Source/Engine/World/World.h)
- [World.cpp](../KnotEngine/Source/Engine/World/World.cpp)
- [Level.h](../KnotEngine/Source/Engine/World/Level.h)
- [Level.cpp](../KnotEngine/Source/Engine/World/Level.cpp)
- [Node.h](../KnotEngine/Source/Engine/World/Node.h)
- [Node.cpp](../KnotEngine/Source/Engine/World/Node.cpp)
- [Component.h](../KnotEngine/Source/Engine/Component/Component.h)
- [Component.cpp](../KnotEngine/Source/Engine/Component/Component.cpp)
- [TransformComponent.h](../KnotEngine/Source/Engine/Component/TransformComponent.h)
- [TransformComponent.cpp](../KnotEngine/Source/Engine/Component/TransformComponent.cpp)
- [EditorEngine.h](../KnotEngine/Source/Editor/Runtime/EditorEngine.h)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/Runtime/EditorEngine.cpp)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [Input-Architecture.md](Input-Architecture.md)
- [Conventions.md](Conventions.md)
