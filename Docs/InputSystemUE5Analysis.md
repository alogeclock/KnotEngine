# Unreal Engine Input System 구현 분석 보고서

작성일: 2026-05-31  
대상 저장소: `C:\Users\Jungle\Desktop\UnrealEngine-release`  
기준 문서: `Unreal_Input_System_Report.md` 및 엔진 소스 직접 분석

## 1. 핵심 결론

Unreal Engine의 입력 시스템은 하나의 단일 파이프라인이 아니라, 여러 소비자가 계층적으로 입력을 볼 수 있는 구조다. 입력은 대략 다음 순서로 이동한다.

```text
OS / Platform Application
-> FSlateApplication
-> Slate InputPreProcessor
-> Slate Widget 경로 Preview/Tunnel, Bubble
-> SViewport / FSceneViewport
-> UGameViewportClient
-> UConsole, Viewport override delegate, LocalPlayer
-> APlayerController::InputKey
-> UPlayerInput / UEnhancedPlayerInput
-> APlayerController::ProcessPlayerInput
-> UInputComponent 스택
-> Actor, Controller, LevelScript, Pawn, Enhanced Input 바인딩
```

중요한 점은 입력이 항상 게임플레이까지 내려가지 않는다는 것이다. Slate 위젯이 `FReply::Handled()`를 반환하면 UI 계층에서 멈출 수 있고, `UGameViewportClient`의 콘솔이나 override delegate가 처리하면 PlayerController까지 내려가지 않을 수 있다. PlayerController까지 내려온 뒤에도 `InputComponent` 스택의 우선순위, `bConsumeInput`, `bBlockInput`, Enhanced Input의 Mapping Context 우선순위와 Action 소비 옵션에 따라 아래 계층으로의 전달이 제한된다.

## 2. 입력을 받는 주요 소비자

### 2.1 Slate Input PreProcessor

`FSlateApplication::ProcessKeyDownEvent`는 위젯에 키 이벤트를 보내기 전에 `InputPreProcessors.HandleKeyDownEvent`를 먼저 호출한다. 코드 주석도 Analog Cursor가 입력을 먼저 볼 기회를 가진다고 설명한다. 즉 입력 전처리기는 Slate 위젯보다 앞선 소비자다.

이 계층은 아날로그 커서, 에디터/플러그인 입력 가로채기, 원격 입력 같은 시스템이 게임/위젯보다 먼저 입력을 관찰하거나 소비할 수 있는 자리다.

### 2.2 Slate / UMG 위젯

Slate 위젯은 키보드 포커스 또는 포인터 hit-test 경로를 기준으로 입력을 받는다. 포인터 입력은 먼저 preview/tunnel 단계로 부모에서 자식 방향으로 흐르고, 처리되지 않으면 bubble 단계로 자식에서 부모 방향으로 흐른다.

마우스 down의 실제 흐름은 `RoutePointerDownEvent` 안에서 다음과 같다.

```text
OnPreviewMouseButtonDown
-> 처리 안 됨
-> OnTouchStarted 또는 OnMouseButtonDown
```

키보드 down도 같은 철학을 따른다. 포커스 경로를 따라 먼저 `OnPreviewKeyDown`을 tunnel로 보내고, 처리되지 않으면 `OnKeyDown`을 bubble로 보낸다.

UMG는 내부적으로 Slate 위젯 위에 얹힌다. 따라서 `UUserWidget::NativeOnMouseButtonDown`, Blueprint의 `OnMouseButtonDown`, `OnPreviewMouseButtonDown` 같은 이벤트도 결국 이 Slate 라우팅 규칙의 영향을 받는다.

### 2.3 SViewport / FSceneViewport

게임 화면 자체도 Slate 위젯이다. `SViewport`가 이벤트를 받으면 실제 게임 뷰포트 구현인 `FSceneViewport`가 `OnMouseButtonDown`, `OnKeyDown`, `OnKeyUp` 등에서 입력을 `ViewportClient->InputKey`로 넘긴다.

예를 들어 마우스 버튼 down은 `FSceneViewport::OnMouseButtonDown`에서 `ViewportClient->InputKey(... IE_Pressed ...)`로 전달되고, 키 down은 `FSceneViewport::OnKeyDown`에서 `IE_Pressed` 또는 `IE_Repeat`로 전달된다.

`FSceneViewport`는 단순 전달자가 아니다. 마우스 캡처, 포커스 획득, 커서 숨김, 고정밀 마우스 입력, viewport lock을 `FReply`에 담아 Slate에 요청한다.

즉 게임 입력도 먼저 Slate 위젯 이벤트이며, `FSceneViewport`가 “Slate 이벤트를 게임 viewport 입력으로 번역하는 경계” 역할을 한다.

### 2.4 UGameViewportClient

`UGameViewportClient::InputKey`는 게임 viewport 단계의 중심 소비자다. 실제 순서는 다음에 가깝다.

```text
TryToggleFullscreenOnInputKey
-> RemapControllerInput
-> IgnoreInput 검사
-> OnInputKeyEvent.Broadcast
-> 에디터 GameViewportInputKeyDelegate
-> ViewportConsole
-> OnOverrideInputKeyEvent
-> Target LocalPlayer의 PlayerController::InputKey
```

축 입력도 유사하게 `InputAxis`에서 콘솔, override delegate, PlayerController로 내려간다.

이 단계에서 소비자가 많은 이유는 viewport가 게임, 콘솔, 에디터 PIE, split-screen/local multiplayer, 원격 입력 플러그인 같은 시스템의 교차점이기 때문이다.

### 2.5 APlayerController / UPlayerInput

`APlayerController::InputKey`는 즉시 바인딩 함수를 호출하지 않는다. 기본적으로 `PlayerInput->InputKey`에 키 상태를 넣고, 실제 바인딩 실행은 매 프레임 `APlayerController::ProcessPlayerInput`에서 수행한다.

`ProcessPlayerInput`은 `BuildInputStack`으로 현재 처리할 `UInputComponent` 목록을 만들고, `PlayerInput->ProcessInputStack`에 넘긴다.

`UPlayerInput::ProcessInputStack`은 키 상태를 평가하고, 입력 델리게이트 후보를 모아 실행한다.

### 2.6 UInputComponent 스택의 소비자들

`UInputComponent`는 Action, Key, Touch, Gesture, Axis, AxisKey, VectorAxis 바인딩을 갖는다.

Actor가 `EnableInput`을 호출하면 자신의 `InputComponent`가 생성되고 PlayerController의 `CurrentInputStack`에 push된다. 이때 Actor의 `bBlockInput`과 `InputPriority`가 InputComponent에 복사된다.

Pawn은 possession 시 `CreatePlayerInputComponent`와 `SetupPlayerInputComponent`를 통해 입력 컴포넌트를 구성한다.

### 2.7 Enhanced Input 소비자

Enhanced Input은 `UEnhancedPlayerInput`, `UEnhancedInputComponent`, `UEnhancedInputLocalPlayerSubsystem`, `UInputMappingContext`, `UInputAction`이 함께 구성한다.

`UEnhancedInputComponent`는 여전히 `UInputComponent` 스택에서 처리된다. 헤더 주석도 PlayerController가 관리하는 스택을 PlayerInput이 처리한다고 명시한다.

다만 Legacy와 달리 키 바인딩이 직접 함수에 연결되는 것이 아니라, 먼저 Mapping Context가 Action으로 번역되고, Action의 Modifier/Trigger 평가 결과가 `UEnhancedInputComponent`의 `BindAction` 델리게이트에 전달된다.

## 3. Slate가 입력을 소비하는 방식

Slate의 입력 소비 단위는 `FReply`다. 위젯 핸들러가 `FReply::Handled()`를 반환하면 해당 라우팅 단계에서 이벤트가 처리된 것으로 간주된다.

### 3.1 키보드 입력

키보드는 포인터 hit-test가 아니라 사용자 포커스 경로를 사용한다.

```text
FSlateApplication::ProcessKeyDownEvent
-> InputPreProcessors
-> SlateUser->GetFocusPath()
-> OnPreviewKeyDown tunnel
-> OnKeyDown bubble
-> FReply handled 여부 반환
```

Preview 단계가 처리하면 bubble 단계는 실행되지 않는다.

### 3.2 포인터 입력

마우스/터치 down은 현재 포인터 아래 위젯 경로를 만든 뒤 다음 순서로 라우팅된다.

```text
hit-test 결과 FWidgetPath
-> OnPreviewMouseButtonDown tunnel
-> 처리 안 됨
-> OnTouchStarted 또는 OnMouseButtonDown bubble
```

### 3.3 Viewport가 Slate와 게임 입력을 연결하는 방식

Viewport가 focus/capture를 갖고 있거나 capture 정책상 입력을 처리해야 하면 `FSceneViewport`가 `ViewportClient->InputKey`를 호출한다. 호출 결과가 false면 `CurrentReplyState`를 `Unhandled`로 바꾼다.

따라서 UI 위젯이 viewport 위에서 입력을 처리하면 게임까지 내려가지 않고, viewport가 입력을 받으면 그때부터 `UGameViewportClient` 이후의 게임 입력 파이프라인이 동작한다.

## 4. InputComponent 우선순위와 소비 규칙

### 4.1 스택 구성 순서

`APlayerController::BuildInputStack`은 기본적으로 다음 순서로 배열에 넣는다.

```text
Pawn InputComponent
-> Pawn의 기타 UInputComponent
-> LevelScriptActor InputComponent
-> PlayerController InputComponent
-> PushInputComponent로 들어온 CurrentInputStack
-> Priority 기준 stable sort
```

정렬은 `Priority` 오름차순이다. 그러나 실제 처리는 `UPlayerInput::EvaluateInputDelegates`에서 배열 끝에서 처음으로 역순 순회한다. 그래서 결과적으로 높은 `Priority`가 먼저 처리된다.

### 4.2 bConsumeInput

`FInputBinding`의 기본값은 `bConsumeInput = true`다.

Action, Key, Touch, Gesture, Axis 계열 바인딩이 소비를 요청하면 해당 키가 `KeysToConsume`에 추가되고, 컴포넌트 평가가 끝난 뒤 `KeyState->bConsumed = true`로 표시된다. 다음 낮은 우선순위 컴포넌트는 이 키를 이미 소비된 것으로 보고 바인딩 후보에서 제외한다.

소비는 “컴포넌트 전체 평가가 끝난 뒤” 적용된다. 같은 `InputComponent` 안의 여러 바인딩이 같은 키를 볼 수 있게 하려는 의도다.

### 4.3 bBlockInput

`bBlockInput`은 개별 키가 아니라 스택 순회를 멈추는 더 강한 규칙이다. `EvaluateInputComponentDelegates`가 block을 반환하면 `EvaluateInputDelegates`는 스택 순회를 중단하고, 남은 낮은 우선순위 컴포넌트의 축 값을 0으로 정리한다.

정리하면 다음과 같다.

| 규칙 | 범위 | 효과 |
| --- | --- | --- |
| `bConsumeInput` | 특정 키/바인딩 | 같은 키가 낮은 우선순위 컴포넌트로 내려가는 것을 막음 |
| `bBlockInput` | 전체 InputComponent | 낮은 우선순위 컴포넌트 평가 자체를 중단 |
| `Priority` | InputComponent 단위 | 높은 값이 먼저 처리됨 |

## 5. Enhanced Input의 실제 처리

### 5.1 Mapping Context 등록과 우선순위

`IEnhancedInputSubsystemInterface::AddMappingContext`는 `UEnhancedPlayerInput::AppliedInputContextData`에 Mapping Context와 Priority를 저장하고 control mapping rebuild를 요청한다.

Rebuild 시에는 적용된 Context를 `Priority` 내림차순으로 정렬한다. 즉 높은 우선순위의 Mapping Context가 먼저 적용된다.
각 Context의 Mapping을 순회하면서 이미 상위 Context에서 소비된 key는 `AppliedKeys` 때문에 낮은 Context에서 같은 key mapping을 추가하지 않는다. 단 같은 Context 안에서는 `ContextAppliedKeys`를 나중에 append하므로 같은 Context 내부의 여러 mapping은 허용된다.

### 5.2 Modifier와 Trigger 평가

`UEnhancedPlayerInput::PrepareInputDelegatesForEvaluation`는 `EnhancedActionMappings`를 순회하며 key state를 찾고, `ProcessActionMappingEvent`를 호출한다.

`ProcessActionMappingEvent`는 raw key value에 mapping-level Modifier를 적용하고, mapping-level Trigger를 평가한 뒤 ActionData에 값을 누적한다.
여러 입력이 같은 Action에 연결될 때 값 누적은 Action의 `AccumulationBehavior`에 따라 달라진다. 기본은 절댓값이 큰 컴포넌트를 채택하고, Cumulative는 더한다. WASD처럼 +1/-1이 서로 상쇄되어야 하는 경우 Cumulative가 의미 있다.

이후 action-level Modifier와 Trigger를 다시 평가해 최종 `ETriggerEvent`를 만든다.

### 5.3 Enhanced InputComponent 바인딩 실행

`UEnhancedPlayerInput::EvaluateInputComponentDelegates`는 `UEnhancedInputComponent`만 골라 ActionEventBinding을 검사한다. Binding의 ActionData가 현재 바인딩된 `ETriggerEvent`와 맞으면 delegate를 복제해 실행 큐에 넣는다.

`Started`와 `Triggered`가 한 프레임에 동시에 발생하면 `Started` delegate를 앞에 넣어 먼저 실행되게 한다.

Enhanced Input의 listener consumption은 `FEnhancedInputActionEventBinding::bConsumes`와 `ConsumedInputActions`로 관리된다. 높은 우선순위 InputComponent의 listener가 소비하면 낮은 우선순위 listener는 같은 Action을 실행하지 않는다.

다만 헤더 주석은 Enhanced Input Component 바인딩 자체는 기존 입력 이벤트를 소비하지 않으며, 이 동작은 Mapping Context priority로 재현한다고 설명한다.

이 말은 두 층을 구분해서 이해해야 한다.

```text
Mapping Context priority / UInputAction::bConsumeInput
-> 어떤 key-to-action mapping이 살아남는지 결정

EnhancedInputComponent listener consumption
-> 같은 Action에 바인딩된 낮은 우선순위 listener 실행을 막음

UInputAction::bConsumesActionAndAxisMappings
-> Enhanced Action이 기존 Legacy Action/Axis 바인딩을 소비할지 결정
```

Legacy 키 소비와 연결되는 부분은 RebuildControlMappings에서 `KeyConsumptionData`를 채우는 코드다.

## 6. 개별 입력 종류별 처리 우선순위

### 6.1 키보드 키

```text
InputPreProcessor
-> Slate focused widget PreviewKeyDown
-> Slate focused widget KeyDown
-> SViewport/FSceneViewport
-> GameViewportClient
-> Console / Override / LocalPlayer
-> PlayerController::InputKey
-> PlayerInput key state
-> 다음 ProcessPlayerInput에서 InputComponent 스택 처리
```

키 down/up은 즉시 바인딩이 실행되는 것이 아니라 key state와 event count로 누적되고, 프레임 입력 처리에서 action/key binding 후보가 만들어진다.

### 6.2 마우스 버튼

```text
InputPreProcessor
-> Hit-test path PreviewMouseButtonDown
-> Hit-test path MouseButtonDown
-> SViewport/FSceneViewport
-> ViewportClient::InputKey
-> 이후 키 입력과 동일
```

마우스 버튼은 Slate 위젯이 UI 클릭으로 소비할 가능성이 높다. viewport가 처리하는 경우에도 capture 정책과 현재 focus/capture 상태가 중요하다.

### 6.3 마우스 이동 / 축

마우스 이동은 Slate에서는 pointer move 이벤트이고, viewport가 게임 입력으로 처리하면 `InputAxis` 또는 mouse delta 성격으로 내려간다. `UGameViewportClient::InputAxis`는 콘솔/override/PlayerController 순서로 처리한다.`

Legacy Axis binding은 매 프레임 `DetermineAxisValue`로 값을 계산하고 delegate를 모은다.

### 6.4 터치 / 제스처

터치는 Slate pointer event의 한 종류로 먼저 위젯에 전달된다. `RoutePointerDownEvent`는 touch인 경우 `OnTouchStarted`를 먼저 호출하고, 처리되지 않으면 설정에 따라 mouse fallback으로 `OnMouseButtonDown`도 호출할 수 있다.

게임 입력으로 내려온 뒤에는 `UInputComponent`의 `TouchBindings`, `GestureBindings`에서 별도로 평가된다.

### 6.5 게임패드

게임패드는 `UGameViewportClient`에서 PIE 다중 창 라우팅, controller remapping, LocalPlayer 선택의 영향을 크게 받는다. 실제 대상 PlayerController는 `GEngine->GetLocalPlayerFromInputDevice`로 선택된다.

Enhanced Input에서는 key state가 analog인지, pressed/released event count가 있는지, held인지에 따라 `EKeyEvent::Actuated`, `Held`, `None`으로 변환되어 Trigger 평가에 들어간다.

## 7. 설계 관점에서 본 우선순위 모델

Unreal 입력 우선순위는 하나의 숫자만으로 결정되지 않는다. 실제로는 다음 다섯 층이 겹친다.

| 층 | 우선순위 기준 | 소비 방식 |
| --- | --- | --- |
| Slate 전처리 | InputPreProcessor 등록/순회 | true 반환 |
| Slate 위젯 | focus path 또는 hit-test path, tunnel/bubble | `FReply::Handled()` |
| Viewport | capture/focus/input mode | `ViewportClient->InputKey` 결과와 `FReply` |
| GameViewportClient | console, override, local player routing | bool 처리 결과 |
| PlayerInput | InputComponent priority, consume/block | `bConsumeInput`, `bBlockInput`, consumed key |
| Enhanced Input | Mapping Context priority, Action consume, Trigger event | mapping suppression, action listener consumption |

실무적으로 입력 버그를 볼 때는 “키가 바인딩됐는가”보다 “어느 계층에서 먼저 소비됐는가”를 찾아야 한다.

## 8. 디버깅 체크리스트

1. UI 위젯이 `FReply::Handled()`를 반환해 viewport까지 입력이 내려가지 않는지 확인한다.
2. viewport가 focus/capture 상태인지 확인한다.
3. `UGameViewportClient::IgnoreInput`, 콘솔 활성화, override delegate가 입력을 먹는지 확인한다.
4. 입력 장치가 올바른 `ULocalPlayer`로 매핑되는지 확인한다.
5. `APlayerController::BuildInputStack`에서 원하는 `InputComponent`가 스택에 있는지 확인한다.
6. `InputComponent->Priority`, `bBlockInput`, 각 binding의 `bConsumeInput`을 확인한다.
7. Enhanced Input 사용 시 Mapping Context가 추가되었고 priority가 의도한 순서인지 확인한다.
8. 같은 key가 높은 priority Mapping Context에서 `bConsumeInput`으로 낮은 context를 막는지 확인한다.
9. `UInputAction::bConsumesActionAndAxisMappings` 때문에 legacy binding이 소비되는지 확인한다.
10. `showdebug enhancedinput`, `showdebug devices`로 런타임 상태를 확인한다.

## 9. 최종 요약

Unreal Engine의 Input System은 OS 입력을 곧장 Pawn 함수로 호출하는 구조가 아니다. 먼저 Slate가 입력을 UI 이벤트로 소비할 기회를 갖고, 게임 viewport가 선택된 경우에만 `UGameViewportClient`와 `PlayerController`로 내려간다. `PlayerController`는 입력 상태를 `PlayerInput`에 축적하고, 매 프레임 `InputComponent` 스택을 높은 priority부터 처리한다. Legacy Input은 키/액션/축 바인딩과 `bConsumeInput`, `bBlockInput`이 핵심이고, Enhanced Input은 여기에 Mapping Context priority, Modifier, Trigger, Action 단위 소비 규칙을 추가한다.

결론적으로 입력 소비자는 UI, viewport, console, controller, pawn만이 아니다. Slate 전처리기, 각종 viewport delegate, LocalPlayer routing, Actor input component, LevelScript, Enhanced Input subsystem, 플러그인 input processor까지 모두 입력 소비자가 될 수 있다. 입력 우선순위는 “Slate 라우팅 우선순위”, “Viewport 라우팅 우선순위”, “InputComponent 스택 우선순위”, “Enhanced Mapping Context 우선순위”가 단계별로 합성된 결과로 이해해야 한다.
