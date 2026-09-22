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
- 플레이 생명주기는 소유 계층을 따라 전달하고, Tick은 Level별 밀집 Component 배열을 순회한다.
- Editor는 WorldContext ID로 렌더링할 World를 선택한다.
- World.Tick 끝에서 Scene을 갱신하며 이 단계는 Stopped·Paused 상태에서도 실행한다.
- Dirty 상태는 Proxy가 보관한다. World는 Dirty Component 목록을 관리하지 않는다.
- 플레이 상태 전환은 EditorEngine만 요청하며 World Tick 도중에는 전환하지 않는다.

## 전체 구조

```text
UEngine
└─ FWorldContext[]
   └─ UWorld
	  ├─ FScene → FPrimitiveSceneProxy[]
	  ├─ PersistentLevel ─┐
	  └─ ULevel[] <───────┘
		 ├─ UNode[]
		 │  ├─ UTransformComponent  정확히 하나
		 │  └─ UComponent[]         Transform을 포함한 합성 기능
		 └─ TickComponents[]        현재 Tick 가능한 Component의 비소유 밀집 배열
```

```text
소유 계층
UWorld → ULevel → UNode → UComponent

공간 계층
UTransformComponent Parent → Child TransformComponents
```

Node의 Transform 부모를 바꿔도 Level의 `Nodes` 배열 소속은 바뀌지 않는다. 같은 Node가 두 Component를 소유한다는 사실도 Component 사이의 실행 순서를 정의하지 않는다.

## 디렉터리와 책임

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
   ├─ PrimitiveComponent.h/.cpp
   ├─ Mesh/
   │  └─ StaticMeshComponent.h/.cpp
   └─ MovementComponent.h/.cpp
```

| 계층 | 현재 책임 |
|---|---|
| `UEngine` | WorldContext 생성, 조회, 파괴와 World 참조 수집 |
| `FWorldContext` | Context ID, World 종류와 World 참조 보관 |
| `UWorld` | Level 수명, PersistentLevel·플레이 상태, Scene 소유와 갱신 시점 |
| `ULevel` | Node의 평탄한 소유, 생명주기 전달과 Tick Component 밀집 배열 실행 |
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

`UWorld`는 하나의 독립적인 시뮬레이션을 나타내는 최상위 객체다. Level 수명과 플레이 상태를 관리하고 Tick을 모든 Level에 전달하며 FScene을 소유한다.

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

네 전환 함수는 `UWorld`의 private 함수이며 플레이 상태 전환 권한은 friend인 `UEditorEngine`에만 부여한다. EditorEngine은 Startup, Level 교체와 Shutdown처럼 `UWorld::Tick()` 바깥의 명확한 경계에서 상태를 즉시 전환한다. Component가 World에 전환을 요청하는 공개 API나 `RequestedPlayState`를 두지 않으며, 한 번의 World Tick 동안 PlayState는 바뀌지 않는 것을 현재 계약으로 삼는다. `UWorld` 소멸자는 종료 안전성을 위해 내부에서 `EndPlay()`를 직접 호출한다.

## ULevel

`ULevel`은 World에 속한 Node의 저장 및 관리 단위다. 생성 시 받은 World를 `OwningWorld`로 참조하고 소속 Node를 `Nodes` 배열에 평탄하게 소유한다.

Level은 Transform 공간 계층의 루트가 아니다. 서로 부모·자식인 Node도 Level 배열에는 각각 독립된 원소로 들어간다.

```text
ULevel::Nodes
├─ Node A
├─ Node B  Transform parent: A
└─ Node C  Transform parent: B
```

`CreateNode()`로 만든 Node는 즉시 배열에 들어간다. World가 `Stopped` 상태가 아니면 새 Node도 곧바로 BeginPlay를 받는다. Level의 Node 배열은 밀집 저장소이며 제거 시 내부 인덱스로 swap-pop한다. 따라서 UUID와 객체 주소는 식별에 사용할 수 있지만 배열 순서는 안정적이지 않다. World는 BaseName별 접미사 카운터로 새 Node의 표시 이름을 만든다.

Transform의 `Children` 배열과 Level의 비소유 `RootNodes` 배열이 Hierarchy 순서를 관리한다. 각 Transform은 배열상의 `SiblingIndex`를 캐시하므로 조회와 저장은 O(1)이며, 부모 변경이나 순서 변경 때 영향받은 형제 구간만 인덱스를 갱신한다. `Nodes`는 계속 모든 Node를 평탄하게 소유하고 제거 시 swap-pop하므로 Hierarchy 순서와 분리된다. `RemoveNode()`는 Transform 자손을 먼저 재귀적으로 제거하고 대상 Node를 제거한다.

현재 Level은 다음 생명주기 요청을 모든 Node에 전달한다.

- `BeginPlay()`
- `EndPlay()`

`Tick()`은 Node에 전달하지 않는다. Level은 실행 조건을 만족한 Component만 `TickComponents` 비소유 밀집 배열에 보관하고 이 배열을 직접 순회한다. Component 등록과 해제는 Component 자신이 가진 배열 인덱스를 사용하며, 해제 시 swap-pop으로 빈자리를 채운다.

## UNode와 Component 합성

`UNode`는 Level에 배치되는 최소 객체이자 Component 합성 단위다. `UNode`는 `final`이며 게임 기능을 Node 상속으로 확장하지 않는다.

모든 Node는 생성과 함께 정확히 하나의 `UTransformComponent`를 만든다. Transform은 빠른 접근을 위한 `Transform` 멤버와 소유를 나타내는 `Components` 배열 양쪽에서 참조한다.

`UTransformComponent`는 상대 위치, `FRotator RelativeRotation`, 상대 크기를 직렬화 원본으로 보관한다. `FQuat CachedRotation`은 `RelativeRotation`에 대응하는 정규화된 계산용 캐시다. Inspector에서 입력한 360도 이상의 회전값은 `RelativeRotation`에 그대로 남고, Gizmo처럼 Quaternion 결과를 적용하는 경로는 기존 회전의 winding과 가까운 동등 표현을 선택한 뒤 실제 변화량만 누적한다. 렌더링과 행렬 계산에는 `CachedRotation`을 사용한다.

Transform 계층은 Parent와 Children 포인터를 Transient로 관리한다. `.kmap` Version 1은 Level별 Node를 평탄하게 저장하고 각 Node에 `ParentUUID`와 `SiblingIndex`를 필수로 기록한다. 이 포맷 이전에 생성된 `.kmap`은 하위 호환하지 않는다. Load는 모든 Parent 참조, Level 경계, 순환과 Sibling Index 연속성을 먼저 검증한 뒤 임시 World에 전체 객체 그래프를 복원한다. 복원이 모두 성공한 경우에만 기존 World의 Level/Node 상태를 교체한다.

```text
Cube UNode
├─ UTransformComponent
├─ UCubeComponent → /Engine/Model/Cube/Cube
└─ UMovementComponent
```

Cube, Sphere, Quad, Cylinder, Capsule Component는 `UStaticMeshComponent`를 상속한다. 각 Source와 Static Mesh는 `Content/Engine/Model/<Shape>/<Shape>.glb|.kasset`에 함께 두며 Component는 이 경로의 Static Mesh를 선택한다. `FAssetManager`는 `LoadStaticMesh()`로 파일을 로드하고 `FindStaticMesh()`로 이미 등록된 Asset만 조회한다.

`AddComponent<UTransformComponent>()`는 컴파일 시 금지된다. `RemoveComponent()`도 Transform 제거를 거부하므로 공개된 Node에는 항상 하나의 Transform이 존재한다.

일반 Component는 Owner Node를 통해 Level과 World에 접근한다.

```text
UComponent::GetWorld
	↓
UNode::GetWorld
	↓
ULevel::GetWorld
```

Node에 붙은 Component는 Owner가 설정된 뒤 즉시 등록된다. Component는 `Components` 배열 순서로 BeginPlay와 EndPlay를 받지만 Tick 순서는 이 배열과 무관하다. Tick 가능한 Component는 활성 상태에 진입할 때 Level의 `TickComponents`에 등록된다. Node 파괴 시 등록을 해제한 뒤 Component를 역순으로 파괴한다. Transform이 가장 먼저 생성되므로 다른 Component보다 나중에 파괴된다.

## UComponent

`UComponent`는 공간 정보와 렌더링 상태를 직접 갖지 않는 공통 기반이다. Owner Node, World와 필수 Transform에 접근할 수 있다.

현재 공통 상태는 다음과 같다.

| 상태 | 의미 |
|---|---|
| Owned | `Owner != nullptr`에서 파생되는 Node 소유 상태 |
| `bIsRegistered` | Component가 Owner의 World에 등록된 상태 |
| `bHasBegunPlay` | 현재 World의 플레이 생명주기에 진입한 상태 |
| `bIsActive` | 플레이 중 Component 기능이 활성화된 상태 |
| `bCanEverTick` | 파생 Component가 Tick callback을 제공한다는 생성 시점 capability |
| `bTickEnable` | Tick 등록을 허용하는 Inspector 설정 |
| Tick Registered | `TickComponentIndex`가 유효하여 Level의 밀집 배열에 들어간 상태 |

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

PrimitiveComponent의 등록 훅은 World.Scene에 연결되어 있다. Tick은 공통 상태가 바뀔 때 `UpdateTickRegistration()`을 통해 Level Registry에 연결되며 Physics 연결은 향후 추가한다.

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

#### Tick capability와 활성화

`bCanEverTick`과 `bTickEnable`은 역할이 다르다. `bCanEverTick`은 파생 Component 생성자가 설정하는 capability이며 Inspector에 노출하지 않는다. 기본값은 false다. `bTickEnable`은 사용자가 실행 여부를 선택하는 설정이며 기본값은 false다. 예를 들어 `UMovementComponent`는 생성자에서 두 값을 true로 설정한다.

현재 Tick Registry 등록 조건은 다음과 같다.

```text
bCanEverTick
&& bTickEnable
&& Registered
&& BegunPlay
&& Active
```

조건을 만족하면 Component를 소속 Level의 `TickComponents` 끝에 한 번만 추가하고, 하나라도 만족하지 않으면 swap-pop으로 제거한다. Tick Registered는 별도 bool이 아니라 `TickComponentIndex != InvalidTickComponentIndex`에서 파생한다. Inspector가 `bTickEnable`을 바꾸면 `PostEditProperty()`가 즉시 등록 상태를 동기화한다.

현재 Registry는 실행 가능한 Component만 보관하므로 World가 `Playing`일 때 별도의 상태 검사 없이 callback을 호출한다. 향후 `FWorldExecutionManager`가 추가되면 capability, enable과 실행 상태를 TickFunction 및 실행기 상태로 분리한다.

### 상태 불변식

Component 상태는 다음 관계를 항상 만족해야 한다.

```text
BegunPlay → Registered → Owned
Active → BegunPlay
!Registered → !BegunPlay && !Active
Tick Registered → bCanEverTick && bTickEnable && Registered && BegunPlay && Active
```

Tick을 비활성화한 Component도 Registered, BegunPlay와 Active 상태를 유지할 수 있지만 Level의 `TickComponents`에는 들어가지 않는다.

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

현재 PrimitiveComponent의 Register 훅은 Proxy를 생성해 Scene에 소유권을 넘긴다. Tick Registry는 `UComponent`의 상태 전이와 연결되어 있고 Physics Registry는 아직 없다.

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

World가 `Stopped`가 아닌 동안 생성한 Node와 부착한 Component는 BeginPlay를 받는다. 현재 CreateLevel은 빈 Level을 소유 목록에 추가하며, 이후 Node 생성 시 플레이 상태가 반영된다. Node 파괴 시 EndPlay를 먼저 전달한 뒤 Component를 역순으로 파괴한다.

Component 부착과 파괴의 전체 순서는 다음과 같다.

```text
AttachComponent
	├─ Owner 설정
	├─ Components 배열에 추가
	├─ RegisterComponent → OnRegister
	└─ World가 플레이 중이면 BeginPlay
	   └─ bAutoActivate이면 Activate
	      ├─ Tick 조건을 만족하면 Level.TickComponents에 등록
	      └─ OnActivated

Node 파괴
	├─ EndPlay → Deactivate
	│  ├─ Level.TickComponents에서 제거
	│  └─ OnDeactivated
	├─ UnregisterComponent → OnUnregister
	└─ Component 역순 파괴
```

## 현재 프레임 실행 순서

`FEngineLoop`는 입력 상태를 갱신한 뒤 Engine Tick을 한 번 호출한다. EditorEngine은 Context ID로 Editor World를 찾고 World Tick과 ViewFamily 구성을 수행하고 메인 스레드에서 동기 렌더링한다.

```text
FEngineLoop::Run
	↓
UEditorEngine::ProcessInput
	↓
UEditorEngine::Tick
	├─ ImGui Frame 구성과 입력 라우팅
	├─ 각 WorldContext의 UWorld::Tick
	│    └─ Level/Component 갱신 후 Scene 갱신
	├─ ViewportClient 카메라 Tick
	├─ Client의 ViewFamily 구성
	└─ ViewFamily 렌더링, ImGui Draw 및 Present
```

현재 World Tick은 Level 소유 배열과 각 Level의 Tick Registry를 순회한다.

```text
UWorld::Tick
	↓ 모든 Level
ULevel::Tick
	↓ TickComponents의 현재 원소
UComponent::TickComponent
```

World가 `Playing`일 때만 Level 순회를 시작한다. Level 배열에는 capability, enable, Registered, BegunPlay와 Active 조건을 모두 만족한 Component만 들어 있으므로 Node와 비실행 Component를 매 프레임 검사하지 않는다.

Tick callback이 자신 또는 다른 Component를 비활성화해 swap-pop이 발생할 수 있으므로 Level은 현재 인덱스에 같은 Component가 남아 있을 때만 인덱스를 증가시킨다. 현재 구현에는 Pending Add/Remove 안전 지점이 없어서 Tick 중 새로 등록된 Component가 같은 프레임 배열 뒤쪽에서 실행될 수 있다. 이러한 동적 변경 규칙의 고정은 Roadmap의 World 실행기 책임이다.

Scene 갱신은 World Tick 마지막 순회가 아니라 Component 변경 시점에 Render Command를 제출하는 방식으로 분리된다. 따라서 Stopped·Paused 상태에서도 Inspector 편집은 변경 명령을 생성해 다음 렌더 요청에 반영된다.

현재 실행 순서는 Level 배열과 `TickComponents` 등록 순서의 결과다. swap-pop 해제로 순서가 바뀔 수 있으며 명시적인 dependency 계약이 아니다.

## 현재 렌더 전달

World/Level/Node/Component의 직접 Render 순회와 전체 Snapshot은 사용하지 않는다. FScene은 PrimitiveId 명령 큐와 Render Thread Proxy를 관리한다.

```text
PrimitiveComponent.OnRegister → PrimitiveId 할당 → Add + All Render Command
Transform/Mesh/Material/Visibility 변경 → 부분 Render Command 제출·병합
FRenderSystem.Render → Render Thread에서 Scene Command 적용
PrimitiveComponent.OnUnregister → Remove Render Command → PrimitiveId 초기화
```

Game/Editor Thread에서 상태 변경 값을 명령으로 복사하고 Render Thread에서 Proxy에 적용한다. 부모 Transform 변경·연결·해제·파괴는 자식 Primitive에 Transform 명령을 전파한다.

Proxy는 Component를 참조하지 않고 Render Command가 소유한 값만 읽는다. SceneRenderer는 Render Thread에서 GetProxies로 렌더 상태만 읽는다.

World는 Level과 Component를 먼저 파괴하여 Proxy 등록을 해제하고 마지막에 Scene을 소멸시킨다. 상세 수명 계약은 [Rendering-Architecture.md](Rendering-Architecture.md)를 따른다.

## 현재 구현의 경계

현재 World Tick과 Render Command 생성은 Game/Editor Thread에서 실행하고, Scene 갱신과 GPU 렌더링은 최대 2개 미완료 프레임으로 제한된 Render Thread에서 실행한다. 다음 기능은 아직 제공하지 않는다.

- Register 훅과 Physics Registry의 실제 연결
- Level visibility와 load 상태에 따른 Component 일괄 재등록
- Level별 밀집 Tick 배열을 대체할 World 단위 실행 Phase와 명시적인 Tick dependency
- 실행 중 등록 및 해제의 안전 지점
- Level, Node와 Component 지연 파괴
- Tick interval과 Paused Tick
- Task Graph와 worker thread 실행
- GPU Fence 기반 자원 지연 해제

World 실행 목표는 [World-Architecture-Roadmap.md](World-Architecture-Roadmap.md)에, Render Thread와 Render Pass 목표는 [Rendering-Architecture.md](Rendering-Architecture.md)에 정의한다.

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
