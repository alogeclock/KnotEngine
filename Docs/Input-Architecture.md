# Knot Engine Input Architecture

## 문서 목적

이 문서는 Knot Engine의 입력 수집, 프레임 스냅샷, ImGui 연동, 에디터 입력 라우팅의 책임과 실행 순서를 정의한다.

Knot Engine은 에디터 UI를 자체 retained-mode 위젯 시스템으로 구현하지 않는다. 에디터 패널, 도킹, 일반 UI 상호작용은 Dear ImGui가 담당하고, ImGui 바깥에서 동작하는 뷰포트·카메라·기즈모의 입력 소유권은 `FEditorInputRouter`가 관리한다.

## 설계 원칙

- Win32 메시지와 타입은 `FWindowsApplication`과 `FWindowsInput` 안에 가둔다.
- `Input` 폴더의 타입은 `HWND`, `WPARAM`, `VK_*`에 의존하지 않는다.
- 입력 수집기는 UI, 에디터 뷰포트, 게임플레이 정책을 알지 않는다.
- ImGui가 메시지를 처리했는지와 관계없이 저수준 입력은 항상 수집한다.
- 한 프레임 동안 수집한 상태와 이벤트를 불변 `FInputSnapshot`으로 전달한다.
- 지속 상태 조회와 발생 순서가 중요한 이벤트 처리를 모두 지원한다.
- 에디터 입력은 스냅샷 전체가 아니라 이벤트 단위로 소비한다.
- ImGui의 capture 플래그는 저수준 입력 수집을 막지 않고 에디터 라우팅 정책에만 사용한다.
- 에디터 입력 소유권은 `FEditorInputRouter` 한 곳에서 관리한다.
- 게임 빌드에는 에디터 라우터를 두지 않는다.
- 현재 엔진은 Windows 전용이므로 범용 플랫폼 Application 계층을 별도로 만들지 않는다.

## 전체 구조

```text
Win32 Message Queue
        ↓
FWindowsApplication::WindowProc
        ↓
FWindowsApplication::ProcessMessage
        ├─ FWindowsInput::ProcessMessage
        ├─ ImGui_ImplWin32_WndProcHandler
        └─ 창 수명 및 크기 처리
        ↓
FWindowsApplication::PumpMessages
        ↓
FWindowsInput::TakeSnapshot
        ↓
FInputSnapshot
        ↓
FEngineLoop::Run
        ↓
UEditorEngine::ProcessInput
        ↓
FEditorInputRouter::BeginFrame
        ↓
ImGui Frame 구성 및 입력 대상 등록
        ↓
FEditorInputRouter::RouteInput
        ├─ ImGui
        ├─ Mouse Capture Target
        ├─ Keyboard Focus Target
        └─ Hovered Target
```

게임 빌드는 에디터 라우터를 통과하지 않는다.

```text
FInputSnapshot
        ↓
UGameEngine::ProcessInput
        ↓ 향후 구현
GameViewportClient
        ↓
LocalPlayer / PlayerController / PlayerInput
```

## 디렉터리와 책임

```text
KnotEngine/Source/
├─ Engine/
│  ├─ Input/
│  │  ├─ InputKeys.h
│  │  ├─ InputEvents.h
│  │  ├─ InputSnapshot.h
│  │  └─ InputSnapshot.cpp
│  └─ Runtime/
│     ├─ WindowsApplication.h/.cpp
│     ├─ WindowsWindow.h/.cpp
│     ├─ WindowsInput.h/.cpp
│     ├─ EngineLoop.h/.cpp
│     └─ Engine.h/.cpp
└─ Editor/
   ├─ Input/
   │  └─ EditorInputRouter.h/.cpp
   ├─ UI/
   │  └─ EditorUISystem.h/.cpp
   └─ Viewport/
```

| 계층 | 책임 | 포함하지 않는 것 |
|---|---|---|
| `FWindowsApplication` | 창, 메시지 펌프, `WindowProc`, 스냅샷 보관 | 키 상태 계산, 패널 입력 정책 |
| `FWindowsInput` | Win32 입력 해석, 상태 누적, 이벤트 생성, Raw Mouse 수집 | ImGui capture, 뷰포트 선택 |
| `FInputSnapshot` | 한 프레임 입력의 읽기 전용 상태와 이벤트 | 소비 여부, 포커스, 캡처 정책 |
| `FEditorUISystem` | ImGui 프레임 구성, 패널 그리기, ImGui capture 상태 전달 | 물리 키 변환, 입력 소유권 보관 |
| `FEditorInputRouter` | 대상 등록, 이벤트별 target 결정, 논리 포커스와 캡처 | Win32 처리, ImGui 위젯 렌더링 |
| `IEditorInputTarget` | 뷰포트·기즈모 등의 이벤트 소비 지점 | 전역 target 선택 |
| `FViewportClient` | 향후 뷰포트 입력을 카메라 또는 게임으로 해석 | OS 메시지 직접 처리 |

## 프레임 실행 순서

### 메시지 수집

`FEngineLoop`는 프레임마다 Windows 메시지 큐를 모두 비운 뒤 스냅샷을 하나 만든다.

```text
FrameTimer::Tick
    ↓
FWindowsApplication::PumpMessages
    ├─ 모든 메시지를 FWindowsInput에 누적
    └─ TakeSnapshot을 정확히 한 번 호출
    ↓
GEngine::ProcessInput
    ↓
GEngine::Tick
```

### 에디터 입력 처리

`UEditorEngine::ProcessInput()`은 입력을 즉시 기능에 전달하지 않는다. 스냅샷을 `FEditorInputRouter::BeginFrame()`에 보관한다.

`UEditorEngine::Tick()`은 다음 순서를 따른다.

1. ImGui `NewFrame()`을 시작한다.
2. 에디터 패널을 구성한다.
3. 패널이 뷰포트 등 엔진 입력 대상을 라우터에 등록한다.
4. ImGui `NewFrame()` 직후와 패널 구성 이후의 `WantCaptureMouse`, `WantCaptureKeyboard`, `WantTextInput`을 라우터에 누적한다.
5. `FEditorInputRouter::RouteInput()`이 보관한 이벤트를 발생 순서대로 처리한다.
6. 에디터 기능과 장면을 갱신하고 렌더링한다.
7. ImGui draw data를 렌더링한다.

이 순서에서는 현재 프레임의 ImGui hover, focus, active item 상태를 확인한 뒤 엔진 기능으로 입력을 전달할 수 있다. ImGui capture 상태를 두 시점에서 누적하므로 텍스트 위젯이 Enter 처리 중 포커스를 해제해도 해당 프레임 입력이 뷰포트로 새지 않는다.

## 입력 데이터 모델

### 키와 버튼

`InputKeys.h`는 플랫폼 독립적인 입력 식별자를 정의한다.

| 타입 | 의미 |
|---|---|
| `EKeyboardKey` | 엔진 키보드 키 |
| `EMouseButton` | Left, Right, Middle, Thumb1, Thumb2 |
| `EMouseButtonMask` | 동시에 눌린 마우스 버튼 집합 |
| `EModifierKeyMask` | Shift, Control, Alt, Super 집합 |

### 이벤트

`FInputEvent`는 다음 구조체를 담는 `std::variant`다.

| 타입 | 의미 |
|---|---|
| `FKeyInputEvent` | 키 Down/Up, modifier, 반복 여부 |
| `FPointerInputEvent` | 이동, Raw 이동, 버튼, 더블 클릭, 휠 |
| `FCharacterInputEvent` | UTF-32 문자 입력 |
| `FFocusInputEvent` | 네이티브 창 포커스 변경 |

이벤트는 Windows 메시지가 들어온 순서대로 `FInputSnapshot::GetEvents()`에 저장된다.

### 상태와 이벤트를 함께 두는 이유

스냅샷은 현재 상태와 순서 보존 이벤트를 모두 제공한다.

- 카메라 이동처럼 유지되는 입력은 `IsKeyDown()` 등의 상태 조회가 적합하다.
- 텍스트, 더블 클릭, 한 프레임 안의 Down/Up은 이벤트 순서가 필요하다.

같은 프레임 안에 키를 눌렀다가 떼면 최종 `KeysDown`은 false지만 `KeysPressed`, `KeysReleased`, 이벤트 배열에는 두 전환이 모두 남는다.

## FInputSnapshot

`FInputSnapshot`은 외부에 const 조회 함수만 제공한다. 값은 `FWindowsInput`만 생성하고 변경할 수 있다.

```cpp
bool IsKeyDown(EKeyboardKey Key) const;
bool WasKeyPressed(EKeyboardKey Key) const;
bool WasKeyReleased(EKeyboardKey Key) const;

bool IsMouseButtonDown(EMouseButton Button) const;
bool WasMouseButtonPressed(EMouseButton Button) const;
bool WasMouseButtonReleased(EMouseButton Button) const;

const FVector2& GetPointerPosition() const;
const FVector2& GetPointerDelta() const;
const FVector2& GetRawPointerDelta() const;
const FVector2& GetWheelDelta() const;
const TArray<FInputEvent>& GetEvents() const;
```

### 스냅샷 이후 수집기 상태

| 데이터 | `TakeSnapshot()` 이후 |
|---|---|
| Held key/button | 다음 프레임까지 유지 |
| Pressed/Released | 초기화 |
| Pointer/Raw/Wheel delta | Zero로 초기화 |
| Pointer position | 마지막 위치 유지 |
| Pending events | 비움 |
| Window focus | 다음 프레임까지 유지 |

`FrameNumber`는 1부터 증가한다. `TakeSnapshot()`은 프레임마다 정확히 한 번 호출해야 한다.

## FWindowsInput

### 플랫폼 경계

`FWindowsInput::ProcessMessage()`는 Win32 메시지 프로토콜을 해석하고 엔진 입력 타입으로 변환한다.

```text
ProcessFocusChange
ProcessKeyDown / ProcessKeyUp
ProcessUtf16Character / ProcessUtf32Character
ProcessMouseMove
ProcessMouseButtonDown / ProcessMouseButtonUp
ProcessMouseWheel
ProcessRawInput
ProcessCaptureChanged
```

Win32 Virtual Key는 `EKeyboardKey`로 변환된다. 좌우 Shift, Control, Alt와 NumPad Enter를 구분하며 지원하지 않는 키는 `Unknown`으로 무시한다.

### 포커스 손실

창이 포커스를 잃으면 다음 상태를 정리한다.

- 눌린 키와 마우스 버튼 해제
- Released 상태 기록
- 포인터와 Raw delta 초기화
- 포인터 위치 무효화
- UTF-16 상위 surrogate 대기 상태 폐기

`FEditorInputRouter`도 같은 `FFocusInputEvent`를 받으면 논리적인 포커스, 캡처, Down/Up 소유권을 모두 해제한다.

### 포인터와 Raw Mouse

| 값 | 출처 | 용도 |
|---|---|---|
| `PointerDelta` | `WM_MOUSEMOVE` 클라이언트 좌표 | UI hover와 일반 드래그 |
| `RawPointerDelta` | `WM_INPUT` 상대 이동 | 에디터/FPS 카메라 회전 |

`FWindowsInput`의 `SetCapture()`는 메인 `HWND`가 창 밖의 버튼 Up까지 받기 위한 네이티브 캡처다. 에디터 기능의 논리적인 소유자는 `FEditorInputRouter::MouseCaptureOwner`가 별도로 관리한다.

## ImGui 입력 처리

`FWindowsApplication::ProcessMessage()`는 입력을 다음 순서로 처리한다.

```text
FWindowsInput::ProcessMessage
        ↓
ImGui_ImplWin32_WndProcHandler
        ↓
FWindowsApplication 창 메시지 처리
```

따라서 ImGui가 메시지를 처리해도 엔진 스냅샷의 held, pressed, released, 이벤트가 누락되지 않는다.

ImGui는 자신의 버튼, 텍스트 입력, 드래그와 창 겹침을 처리한다. `FEditorInputRouter`는 ImGui 위젯의 `ActiveId`를 복제하지 않는다. 다음 플래그만 엔진 입력 차단 정책으로 받는다.

```cpp
ImGuiIO::WantCaptureMouse
ImGuiIO::WantCaptureKeyboard
ImGuiIO::WantTextInput
```

이 값들은 어떤 엔진 뷰포트가 입력을 소유하는지 알려주지 않는다. 뷰포트 패널은 `ImGui::IsItemHovered()` 또는 적절한 window hover 결과를 사용해 자신을 명시적으로 등록해야 한다.

## FEditorInputRouter

### 소유 위치

`FEditorInputRouter`는 `UEditorEngine`의 값 멤버다. 에디터 수명과 함께 생성되고 종료 시 `Reset()`된다.

게임 엔진과 Windows 입력 수집기는 이 클래스를 참조하지 않는다.

### 프레임 단위 대상 등록

뷰포트와 기즈모처럼 ImGui 밖에서 입력을 처리하는 객체는 `IEditorInputTarget`을 구현한다.

```cpp
class IEditorInputTarget
{
public:
    virtual FEditorInputReply OnInputEvent(
        const FInputEvent& Event) = 0;

    virtual void OnKeyboardFocusLost() {}
    virtual void OnMouseCaptureLost() {}
};
```

패널은 ImGui UI를 그린 뒤 대상 상태를 등록한다.

```cpp
InputRouter.RegisterTarget(
    ViewportClient,
    ImGui::IsItemHovered(),
    ImGui::IsWindowFocused());
```

등록은 프레임 단위다. 등록 순서는 겹친 대상의 우선순위로 사용하며 마지막에 등록된 hovered/focused 대상이 우선한다. 대상은 `RouteInput()`이 끝날 때까지 살아 있어야 한다.

### FEditorInputReply

대상은 이벤트 처리 결과와 상태 변경 요청을 함께 반환한다.

```cpp
return FEditorInputReply::Handled()
    .SetKeyboardFocus()
    .CaptureMouse();
```

지원되는 요청은 다음과 같다.

- Handled 또는 Unhandled
- 키보드 포커스 설정 또는 해제
- 논리적인 마우스 캡처 설정 또는 해제

라우터는 대상 콜백이 반환된 뒤 요청을 적용한다.

### target 선택

| 이벤트 | target 선택 순서 |
|---|---|
| Mouse Button | Down 소유자, 캡처 대상, hovered 대상, ImGui |
| Pointer Move/Wheel | 캡처 대상, hovered 대상, ImGui |
| Raw Move | 캡처 대상, ImGui |
| Key | Down 소유자, ImGui keyboard/text, focused 대상 |
| Character | ImGui text/keyboard, focused 대상 |
| Focus Lost | 모든 논리 소유권 해제 |

명시적으로 등록된 hovered 뷰포트는 ImGui 전역 mouse capture 플래그보다 우선한다. 일반 ImGui 패널 위에서는 등록된 엔진 target이 없으므로 ImGui가 이벤트를 소비한다.

### Down/Up 소유권

키와 마우스 버튼별로 Down을 소비한 주체를 기억한다.

```text
ButtonDown → Viewport A가 처리
MouseButtonOwner[Right] = Viewport A

커서가 다른 패널로 이동

ButtonUp → Viewport A로 전달
MouseButtonOwner[Right] 해제
```

ImGui가 Down을 소비한 경우에도 `ImGui` 소유자로 기록한다. 다음 프레임에 capture 플래그가 false가 되더라도 대응 Up이 뷰포트로 새지 않는다.

### 마우스 캡처

`MouseCaptureOwner`는 카메라 회전이나 기즈모 드래그처럼 여러 이벤트에 걸친 에디터 조작을 보존한다.

```text
Right Button Down on Viewport A
        ↓
Viewport A가 CaptureMouse 요청
        ↓
Raw Move와 Pointer Move를 계속 Viewport A에 전달
        ↓
Right Button Up 또는 Focus Lost
        ↓
Capture 해제
```

스냅샷에서 눌린 마우스 버튼이 하나도 없으면 라우터는 남아 있는 논리 캡처를 자동 해제한다.

## 뷰포트 연결 계획

현재 `FEditorInputRouter`와 엔진 프레임 연결은 구현되어 있지만 등록된 뷰포트 입력 대상은 아직 없다.

향후 뷰포트는 다음 경로로 연결한다.

```text
ImGui Viewport Panel
        ↓ RegisterTarget
FEditorInputRouter
        ↓ FInputEvent
FSceneViewport 또는 FViewportClient
        ├─ Editor Camera
        ├─ Gizmo
        └─ PIE Game Viewport
```

Level Editor의 1/2/3/4분할은 하나의 ImGui `Viewport` 패널 안에 여러 엔진 viewport slot을 두고, 각 slot의 실제 이미지 사각형에 대해 별도의 target을 등록한다.

```text
ImGui DockSpace
└─ ImGui Window "Viewport"
   └─ FViewportLayout
      ├─ Perspective target
      ├─ Top target
      ├─ Front target
      └─ Right target
```

패널 도킹은 ImGui Docking이 담당하고, 패널 내부의 월드 뷰포트 분할만 작은 전용 layout으로 구현한다.

## 게임 입력과의 경계

`FEditorInputRouter`는 게임 입력 시스템의 기반 클래스가 아니다.

에디터에서 PIE Game Viewport가 입력 대상이면 해당 viewport target이 처리되지 않은 이벤트를 게임 입력 경로로 넘긴다. 에디터가 아닌 게임 빌드에서는 `UGameEngine`이 스냅샷을 `GameViewportClient`, `LocalPlayer`, `PlayerController` 계층으로 직접 전달한다.

런타임 UI와 게임플레이 사이의 라우팅이 필요해지면 해당 계층의 정책으로 구현한다. ImGui 패널, 에디터 기즈모, 에디터 도킹 정책을 범용 엔진 입력 계층에 넣지 않는다.

## 스레딩과 수명

입력 수집, ImGui UI 구성, 에디터 라우팅은 모두 메인 스레드에서 실행된다.

- `WindowProc`가 `FWindowsInput` 상태를 변경한다.
- `TakeSnapshot()`이 같은 스레드에서 완성된 값을 만든다.
- `FEditorInputRouter`가 스냅샷을 값으로 보관한다.
- 등록된 `IEditorInputTarget`은 해당 프레임의 `RouteInput()`까지 살아 있어야 한다.
- 렌더 스레드나 작업 스레드는 `FWindowsInput`과 라우터를 직접 참조하지 않는다.

다른 스레드가 입력을 필요로 하면 완성된 데이터의 명시적인 복사본을 전달해야 한다.

## 현재 구현 상태

### 구현됨

- Windows 키보드와 마우스 메시지 해석
- 좌우 modifier와 NumPad 키 구분
- UTF-16 surrogate 조합과 UTF-32 문자 이벤트
- 마우스 버튼, 더블 클릭, 세로/가로 휠
- Pointer delta와 Raw Mouse delta
- 창 포커스와 네이티브 포인터 캡처
- 프레임 단위 `FInputSnapshot`
- 순서 보존 `FInputEvent` 배열
- `FEditorInputRouter`의 프레임 입력 보관
- 프레임 단위 target 등록
- hovered, keyboard focus, mouse capture owner 관리
- ImGui mouse/keyboard/text capture 반영
- 키와 버튼의 Down/Up 소유권 추적
- 포커스 손실과 대상 제거 시 소유권 정리
- `UEditorEngine` 프레임 흐름 연결

### 미구현

- 실제 Level Viewport 패널과 target 등록
- `FViewportClient` 입력 인터페이스
- 에디터 카메라 이동과 회전
- 기즈모 입력 우선순위
- 뷰포트 내부 splitter 입력
- PIE Game Viewport 전달
- GameViewportClient 이후 게임 입력 경로
- 게임패드, 터치, 펜, IME composition
- ImGui Platform Multi-Viewports의 추가 `HWND` 입력 수집

## 관련 파일

- [InputKeys.h](../KnotEngine/Source/Engine/Input/InputKeys.h)
- [InputEvents.h](../KnotEngine/Source/Engine/Input/InputEvents.h)
- [InputSnapshot.h](../KnotEngine/Source/Engine/Input/InputSnapshot.h)
- [InputSnapshot.cpp](../KnotEngine/Source/Engine/Input/InputSnapshot.cpp)
- [WindowsInput.h](../KnotEngine/Source/Engine/Runtime/WindowsInput.h)
- [WindowsInput.cpp](../KnotEngine/Source/Engine/Runtime/WindowsInput.cpp)
- [WindowsApplication.h](../KnotEngine/Source/Engine/Runtime/WindowsApplication.h)
- [WindowsApplication.cpp](../KnotEngine/Source/Engine/Runtime/WindowsApplication.cpp)
- [EngineLoop.cpp](../KnotEngine/Source/Engine/Runtime/EngineLoop.cpp)
- [Engine.h](../KnotEngine/Source/Engine/Runtime/Engine.h)
- [EditorEngine.h](../KnotEngine/Source/Editor/EditorEngine.h)
- [EditorEngine.cpp](../KnotEngine/Source/Editor/EditorEngine.cpp)
- [EditorInputRouter.h](../KnotEngine/Source/Editor/Input/EditorInputRouter.h)
- [EditorInputRouter.cpp](../KnotEngine/Source/Editor/Input/EditorInputRouter.cpp)
- [EditorUISystem.h](../KnotEngine/Source/Editor/UI/EditorUISystem.h)
- [EditorUISystem.cpp](../KnotEngine/Source/Editor/UI/EditorUISystem.cpp)
- [Conventions.md](Conventions.md)
