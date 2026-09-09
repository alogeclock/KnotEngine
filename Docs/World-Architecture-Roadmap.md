# Knot Engine World Architecture Roadmap

## 문서 목적

이 문서는 Knot Engine World 구조의 향후 구현 목표와 단계별 계획을 정의한다. 현재 구현의 사실과 실행 경로는 [World-Architecture.md](World-Architecture.md)에서 다룬다.

목표 실행 구조는 UE6 EntityFramework의 여섯 World Phase와 dependency graph를 참고한다.

```text
PrePhysics
→ StartPhysics
→ DuringPhysics
→ EndPhysics
→ PostPhysics
→ EndFrame
```

Knot Engine은 기존 Unreal Tick Scheduler와 호환할 필요가 없다. 전역 execution manager, 기존 Tick Group과 연결하는 TickFunction bridge, continuation과 graph link 같은 UE6의 과도기적 통합 구조는 구현하지 않는다. `UWorld::Tick()`이 World 소유 실행기를 직접 구동한다.

첫 구현은 Game Thread에서 위상 정렬된 작업을 직렬 실행한다. signal propagation, interval, worker thread와 Task Graph는 등록·해제 수명과 Phase 내부 순서가 안정된 뒤 실제 요구에 따라 추가한다.

## 목표 설계 원칙

- 각 `UWorld`가 독립적인 `FWorldExecutionManager`를 소유한다.
- Editor, PIE와 Preview World의 실행 graph는 서로 섞이지 않는다.
- Level은 등록 작업의 소속과 일괄 제거 단위다.
- Component의 객체 존재, World 등록, Active, BeginPlay와 Tick enable 상태를 구분한다.
- 첫 실행 단위는 Component가 소유하는 `FComponentTickFunction`이다.
- 여섯 Phase의 순서는 World 전체 barrier 계약이다.
- 같은 Phase 안의 순서는 `RunAfter` dependency로 표현한다.
- 배열과 등록 순서는 gameplay 실행 순서 계약으로 사용하지 않는다.
- 실행 중 등록과 해제는 현재 순회를 바꾸지 않고 안전 지점에서 반영한다.
- Component와 실행 node의 메모리 소유권을 구분한다.
- 직렬 DAG의 의미를 먼저 검증한 뒤 executor만 Task Graph로 교체한다.
- 실제 두 번째 종류가 생기기 전에는 범용 실행 node 계층을 만들지 않는다.

## 목표 구조

```text
UEngine
└─ FWorldContext[]
   └─ UWorld
      ├─ PersistentLevel ─┐
      ├─ ULevel[] <───────┘
      │  └─ UNode[]
      │     ├─ UTransformComponent
      │     └─ UComponent[]
      └─ FWorldExecutionManager
         ├─ PrePhysics Phase
         ├─ StartPhysics Phase
         ├─ DuringPhysics Phase
         ├─ EndPhysics Phase
         ├─ PostPhysics Phase
         ├─ EndFrame Phase
         ├─ PendingAdds
         ├─ PendingRemoves
         └─ 향후 Phase별 Task Graph
```

```text
KnotEngine/Source/Engine/World/
├─ WorldContext.h
├─ World.h/.cpp
├─ Level.h/.cpp
├─ Node.h/.cpp
└─ Tick/
   ├─ ComponentTickFunction.h/.cpp
   └─ WorldExecutionManager.h/.cpp
```

Tick 실행은 World의 프레임 진행과 Component 실행을 연결하므로 `World/Tick`에 둔다. dependency graph와 Task Graph가 커져도 최상위 `Engine/Tick` 디렉터리로 분리하지 않는다.

## 목표 Component Registration

Component 생명주기는 Unreal 방식의 등록과 플레이 상태를 따른다. Unity식 `OnEnable()`과 `OnStart()` 콜백 계층은 추가하지 않는다.

| 상태 | 의미 |
|---|---|
| Owned | Node가 Component를 소유하며 Owner가 유효함 |
| Registered | 소속 World의 Runtime subsystem에 참가함 |
| Active | Component의 gameplay 기능이 활성 상태임 |
| BegunPlay | 현재 World의 play epoch에 진입함 |
| Tick Registered | Component Tick이 World 실행기에 등록됨 |
| Tick Enabled | 등록된 Tick이 실행 후보가 될 수 있음 |

Owned는 `Owner != nullptr`에서, Tick Registered는 향후 Tick Manager 연결 상태에서 파생한다. 나머지 개념도 의미는 분리하되 중복 bool은 저장하지 않는다.

현재 `RegisterComponent()`, `UnregisterComponent()`, `Activate()`, `Deactivate()`와 대응하는 네 개의 protected virtual 훅은 구현되어 있다. `bIsRegistered`, `bHasBegunPlay`, `bIsActive`도 분리되어 있으며 Node 부착과 파괴 경로에 연결되어 있다.

향후 작업은 Register 훅을 Render, Physics와 Tick Registry에 연결하고 Level 재연결 시 일괄 등록 상태를 전환하는 것이다. `UnregisterComponent()`는 Component 파괴와 별개이며 Tick 비활성화도 Render 및 Physics 등록을 제거하지 않는다.

### 등록 순서

```text
UObject 생성
    ↓
Node 소유 목록에 추가하고 Owner 설정
    ↓
RegisterComponent
    ├─ World 연결
    ├─ OnRegister
    ├─ Render/Physics subsystem 등록
    └─ bCanEverTick이면 Component Tick 등록
        ↓ World가 이미 play 중이면
BeginPlay
```

`RegisterComponent()`는 public non-virtual 함수로 상태 검증과 공통 순서를 고정한다. 파생 Component의 `OnRegister()`와 `OnUnregister()`는 Editor property 변경과 Level 재연결 때문에 여러 번 호출될 수 있어야 한다.

Tick 등록은 BeginPlay와 분리한다. Component가 World에 등록될 때 TickFunction도 등록하고 실행기가 PlayState와 enable 상태로 실제 실행 여부를 결정한다.

### 등록 해제 순서

```text
새 Tick 실행 차단
    ↓
Component Tick unregister 요청
    ↓
실행 중이면 PendingRemoves에 추가
    ↓
Physics Registry 제거
    ↓
Render Registry 제거
    ↓
OnUnregister
    ↓
World 연결 해제
    ↓
안전 지점에서 필요한 객체 파괴
```

Unregister는 이미 실행 중인 callback을 강제로 중단하지 않는다. Component를 파괴하기 전에 실행기가 더 이상 해당 Component를 참조하지 않는지 보장한다.

## 여섯 고정 실행 Phase

```cpp
enum class EWorldExecutionPhase : uint8
{
	PrePhysics,
	StartPhysics,
	DuringPhysics,
	EndPhysics,
	PostPhysics,
	EndFrame,
	Count
};
```

`Count`는 배열 크기와 검증에만 사용한다.

| Phase | 의미 | 대표 작업 |
|---|---|---|
| `PrePhysics` | Physics 입력을 만들기 전 gameplay 갱신 | 입력 이동, AI, animation target 계산 |
| `StartPhysics` | Physics simulation 시작 경계 | command 제출과 simulation 시작 |
| `DuringPhysics` | Physics와 독립적으로 실행 가능한 작업 | Physics 결과를 읽지 않는 계산 |
| `EndPhysics` | Physics 완료를 기다리고 결과를 확정 | simulation wait와 body state 확정 |
| `PostPhysics` | 확정된 Physics 결과를 소비 | 후처리 이동, camera와 gameplay 반응 |
| `EndFrame` | 렌더링이 읽을 최종 World 상태를 확정 | transform flush와 render proxy 갱신 |

한 Phase의 모든 작업과 child task가 완료되어야 다음 Phase로 이동한다. Physics가 아직 없어도 여섯 Phase API는 유지하고 비어 있는 Phase는 즉시 종료한다.

### Transform 계산 위치

초기에는 stand-alone graph와 graph link를 만들지 않는다. Dirty Transform을 모아 같은 flush 함수를 명시적인 World 경계에서 호출한다.

```text
PrePhysics gameplay 완료
    ↓
FlushDirtyTransforms
    ↓
StartPhysics

PostPhysics 완료
    ↓
FlushDirtyTransforms
    ↓
EndFrame render proxy update
```

재사용 가능한 실행 subgraph가 실제로 필요해질 때만 별도 구조를 검토한다.

## FComponentTickFunction

첫 실행 단위는 Component가 멤버로 소유하는 `FComponentTickFunction`이다.

```cpp
struct FComponentTickFunction
{
	UComponent* Target = nullptr;
	FExecutionNodeHandle Handle;
	EWorldExecutionPhase Phase = EWorldExecutionPhase::PrePhysics;
	bool bCanEverTick = false;
	bool bTickEnabled = false;
	bool bTickEvenWhenPaused = false;
};
```

초기 타입에는 다음 필드를 넣지 않는다.

- `EndTickGroup`: 하나의 node는 하나의 Phase 안에서 완료한다.
- `bHighPriority`: 정확한 순서는 dependency로 표현한다.
- `bRunOnAnyThread`: Job System을 연결할 때 thread safety 계약과 함께 추가한다.
- `TickInterval`: 매 프레임 등록과 dependency 수명이 안정된 뒤 추가한다.

TickFunction은 Component가 소유하고 실행기는 non-owning으로 참조한다. 등록된 TickFunction은 이동하거나 복사하지 않는다.

Controller나 World subsystem처럼 Component가 아닌 두 번째 Tick 대상이 생기면 공통 실행 node 또는 object callback 등록 API를 추출한다. 그 전에는 Component 전용 구현을 유지한다.

## FWorldExecutionManager

`FWorldExecutionManager`는 `UWorld`가 값 또는 `TUniquePtr`로 하나만 소유하는 일반 C++ 객체다. 생성 시 자신의 World를 받고 전역 manager나 subsystem을 조회하지 않는다.

`Manager`라는 이름은 이 타입이 별도의 엔진이나 자동 생성되는 World Subsystem이 아니라, 한 World에 등록된 실행 작업과 Phase를 관리한다는 사실을 나타낸다. 향후 공통 `UWorldSubsystem` 수명 체계를 도입하기 전까지 `Subsystem` 기반 클래스와 `GetSubsystem<T>()`는 만들지 않는다.

```cpp
class FWorldExecutionManager
{
public:
	explicit FWorldExecutionManager(UWorld& World);

	FExecutionNodeHandle RegisterComponentTick(ULevel& Level, FComponentTickFunction& TickFunction);
	void UnregisterComponentTick(FExecutionNodeHandle& Handle);
	void SetTickEnabled(FExecutionNodeHandle Handle, bool bEnabled);
	void AddRunAfter(FExecutionNodeHandle Node, FExecutionNodeHandle Prerequisite);
	void RemoveRunAfter(FExecutionNodeHandle Node, FExecutionNodeHandle Prerequisite);

	void StartFrame(float DeltaTime);
	void ExecutePhase(EWorldExecutionPhase Phase);
	void EndFrame();
};
```

첫 구현에서 `RegisterFunction()`, sync node와 signal node까지 일반화하지 않는다.

### 내부 데이터

```text
FWorldExecutionManager
├─ PhaseStates[6]
│  ├─ RegisteredNodes
│  ├─ SortedNodes
│  └─ GraphRevision
├─ Node pool
│  ├─ Generation
│  ├─ RegisteredLevel
│  ├─ TickFunction
│  ├─ RunAfter
│  └─ RunBefore
├─ PendingAdds
├─ PendingRemoves
└─ ExecutionState
```

각 node에 `RegisteredLevel`을 저장한다. 첫 구현에서는 Level 제거 시 node pool을 한 번 순회한다. Level unload 비용이 문제가 될 때만 Level별 index를 추가한다.

## Execution handle

Component나 TickFunction 주소를 외부 식별자로 직접 노출하지 않는다. slot index와 generation을 가진 handle을 반환한다.

```cpp
struct FExecutionNodeHandle
{
	uint32 Index = InvalidIndex;
	uint32 Generation = 0;
};
```

slot을 재사용할 때 generation을 증가시킨다. 모든 public 연산은 index와 generation을 함께 검사하여 제거된 handle이 새 node를 가리키지 않게 한다.

handle destructor는 자동 unregister하지 않는다. Component 등록 해제 순서가 명시적으로 보여야 하고 World가 먼저 파괴되는 종료 경로도 있기 때문이다.

## Dependency graph

초기 API는 `RunAfter`를 기준으로 구현하고 내부에서 역방향 `RunBefore` 목록을 함께 유지한다.

```text
Movement.RunAfter(Input)

Input → Movement
```

`AddRunBefore(A, B)`가 필요하면 `AddRunAfter(B, A)`의 편의 함수로 제공한다.

### Phase 경계 규칙

- 같은 Phase의 두 node는 graph edge로 연결한다.
- 앞 Phase node를 뒤 Phase node의 prerequisite로 지정하면 Phase barrier가 이미 순서를 보장한다.
- 뒤 Phase node를 앞 Phase node의 prerequisite로 지정하면 등록을 거부한다.
- 등록된 node의 Phase는 직접 변경하지 않는다. unregister 후 새 Phase에 다시 등록한다.

### 위상 정렬과 cycle

등록 또는 dependency revision이 바뀐 Phase만 다시 정렬한다. 첫 구현은 Kahn 알고리즘을 사용한다.

```text
모든 node의 in-degree 계산
    ↓
in-degree 0인 node 수집
    ↓
node를 결과에 추가하고 dependant in-degree 감소
    ↓
처리 수 == 활성 node 수인지 검사
```

처리하지 못한 node가 남으면 cycle이다. Debug와 Development에서는 관련 node와 Component를 로그하고 `check`로 중단한다. 임의 순서 실행이나 자동 edge 제거로 오류를 숨기지 않는다.

dependency가 없는 node는 결정적인 디버깅을 위해 registration sequence로 정렬할 수 있다. 이 순서는 gameplay 계약이 아니다.

## 동적 등록과 해제

### 프레임 밖 등록

실행 중이 아닐 때는 node slot과 dependency를 즉시 등록하고 Phase revision을 증가시킨다. 실제 정렬은 다음 `StartFrame()` 또는 해당 Phase 실행 직전에 한 번 수행한다.

### 실행 중 등록

Phase 실행 중 생성한 node는 `PendingAdds`에 넣고 다음 프레임부터 실행한다. 아직 지나지 않은 Phase에도 같은 프레임으로 끼워 넣지 않는다.

`NewlySpawned` Phase와 같은 프레임 편입 반복은 구현하지 않는다. 첫 Tick 시점이 spawn 위치와 현재 iterator에 따라 달라지는 문제를 피한다.

### 실행 중 해제

실행 중 unregister 요청을 받으면 node를 즉시 disabled로 바꾸고 `PendingRemoves`에 넣는다. 정렬된 배열에 남아 있더라도 callback은 호출하지 않는다.

Game Thread 직렬 단계에서는 각 Phase 끝과 `EndFrame()`이 안전 지점이다. Task Graph 도입 후에는 node completion과 Phase barrier를 확인한 뒤 실제 slot과 dependency를 제거한다.

### Level unload

```text
Level unload 요청
    ↓
Level 소속 Component Tick disable
    ↓
모든 Component Unregister
    ↓
PendingRemoves 반영과 completion 확인
    ↓
Node와 Component 파괴
    ↓
Level 파괴
```

## 목표 프레임 실행

```text
UWorld::Tick
    ↓
FWorldExecutionManager::StartFrame
    ├─ 이전 프레임 Pending 변경 반영
    ├─ PlayState와 enable 상태 평가
    └─ revision이 바뀐 Phase 위상 정렬
        ↓
ExecutePhase(PrePhysics)
        ↓
FlushDirtyTransforms
        ↓
ExecutePhase(StartPhysics)
        ↓
ExecutePhase(DuringPhysics)
        ↓
ExecutePhase(EndPhysics)
        ↓
ExecutePhase(PostPhysics)
        ↓
FlushDirtyTransforms
        ↓
ExecutePhase(EndFrame)
        ↓
FWorldExecutionManager::EndFrame
    ├─ Pending 제거 반영
    └─ 지연 파괴 가능 상태 확정
```

모든 활성 Level의 같은 Phase 작업을 하나의 World graph에서 실행한다. Level A의 여섯 Phase를 끝낸 뒤 Level B를 실행하지 않는다.

## Signal propagation

Signal은 작업의 순서가 아니라 실행 필요성을 전파한다.

```text
Ordering dependency
Transform 계산 → Render proxy 갱신

Signal dependency
Transform 변경 ⇢ Render proxy 갱신 필요
```

ordering edge는 연결된 node를 같은 실행 graph에 배치한다. signal edge는 graph를 합치지 않고 dirty 상태만 전달해야 한다.

첫 구현에는 signal graph를 넣지 않는다. subsystem별 dirty queue와 Phase 경계의 batch flush로 시작한다.

```text
Transform 변경
    ↓
DirtyTransforms queue
    ↓
FlushDirtyTransforms
```

다음 요구가 확인되면 signal dependency를 추가한다.

- 여러 update 종류가 같은 changed 상태에 의존한다.
- 매 프레임 polling 비용이 측정된다.
- ordering graph를 합치지 않고 변경을 전달해야 한다.
- signal 발생과 소비 Phase의 프레임 규칙을 정의할 수 있다.

동일 node에 여러 signal이 오면 기본적으로 하나의 dirty bit로 합친다. 소비 node가 이미 실행된 뒤 도착한 signal은 다음 프레임으로 넘긴다. 발생 횟수와 payload가 필요하면 signal bit가 아니라 event queue를 사용한다.

## Tick interval과 Pause

Interval은 dependency graph에서 node를 제거하지 않고 callback 실행만 건너뛴다.

```cpp
float TickInterval = 0.0f;
float RemainingTime = 0.0f;
float AccumulatedDeltaTime = 0.0f;
```

건너뛴 delta time을 누적해 실제 callback에 전달한다. prerequisite가 interval 때문에 쉬어도 dependant는 기본적으로 실행 가능하다. 두 작업이 반드시 같이 실행되어야 한다면 같은 interval 정책을 공유하거나 prerequisite 실행이 dependant에 signal을 보내도록 구성한다.

Paused World에서는 기본 Tick을 실행하지 않는다. `bTickEvenWhenPaused`를 구현할 때 pause delta time의 출처와 Physics Phase의 의미를 함께 정의한다.

## Worker thread와 Task Graph

Phase별 DAG와 cycle 검증을 직렬 실행에서 먼저 완성한다. Task Graph는 같은 graph 데이터를 사용하여 준비된 node를 Job System에 제출한다.

```text
Phase 시작
    ↓
in-degree 0인 ready node 제출
    ↓
node 완료
    ├─ dependant in-degree 감소
    └─ 새 ready node 제출
        ↓
모든 node와 child task 완료
        ↓
Phase barrier 통과
```

Task Graph 단계에서 worker 실행 자격과 completion을 추가한다.

```cpp
bool bRunOnAnyThread = false;
FTaskEvent CompletionEvent;
```

`bRunOnAnyThread`는 thread safety를 만들어 주는 기능이 아니라 작성자가 제공하는 실행 자격 보증이다.

다음 작업은 기본적으로 World 소유 Game Thread에 남긴다.

- UObject, Node와 Component 생성 및 파괴
- Register와 Unregister
- Level 배열과 Transform 부모 관계 변경
- Render 및 Physics subsystem 등록
- 일반 gameplay event 전달

Worker는 immutable snapshot을 읽거나 자신에게 할당된 데이터만 쓴다. 결과는 별도 buffer에 기록하고 명시적인 Phase node에서 Game Thread 상태에 반영한다.

`DuringPhysics`는 Physics를 비동기 실행할 때 의미가 생긴다. DuringPhysics 작업은 진행 중인 Physics state를 읽거나 쓰지 않는다. `EndPhysics` 진입 시 simulation completion을 기다리고 결과를 공개한다.

작은 Component Tick을 각각 worker task로 만들면 scheduler 비용이 더 클 수 있다. 대량 동종 작업은 subsystem의 batch node로 처리한다. 이 사례가 생길 때 Component 전용 등록 API를 일반 function node API로 확장한다.

## 완료 조건

| 단계 | 완료 조건 |
|---|---|
| Component Registration | 재등록과 Level 분리 후 subsystem 및 Tick 참조가 남지 않음 |
| 직렬 Registry | 직접 소유 계층 Tick 없이 기존 Component가 같은 동작을 수행 |
| 동적 변경 | Tick 중 Spawn과 Destroy가 다음 프레임 규칙을 안정적으로 따름 |
| Phase DAG | 명시적인 dependency가 지켜지고 cycle이 진단됨 |
| Interval과 Signal | 실행 조건이 dependency 순서와 독립적으로 검증됨 |
| Task Graph | 직렬 executor와 동일한 결과를 내며 Phase completion이 보장됨 |
| Worker 실행 | thread-safe node만 병렬화되고 World 수명 변경은 Game Thread에 유지됨 |

## 관련 문서

- [World-Architecture.md](World-Architecture.md)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [Input-Architecture.md](Input-Architecture.md)
- [Conventions.md](Conventions.md)

UE6 EntityFramework 소스 분석은 다음 로컬 문서에 정리되어 있다.

```text
C:/Users/Administrator/Desktop/UE6-EntityFramework-Execution-Architecture.md
```
