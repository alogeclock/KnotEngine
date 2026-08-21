# Knot Engine Input Architecture

## 문서 목적

이 문서는 Knot Engine의 입력 시스템에서 현재 구현된 범위, 각 계층의 책임, 프레임 스냅샷의 의미와 앞으로 연결할 Slate 기반 입력 라우팅 구조를 정의한다.

현재 구현은 Windows 데스크톱 입력을 엔진 타입으로 변환하고 프레임 단위의 `FInputSnapshot`을 생성하는 단계까지 포함한다. Slate, 뷰포트, 게임플레이 입력 소비 계층은 목표 구조만 정의되어 있으며 아직 구현되지 않았다.

## 설계 원칙

- Win32 타입과 메시지는 플랫폼 경계인 `FWindowsApplication`과 `FWindowsInput` 안에 가둔다.
- `Input` 폴더의 타입은 `HWND`, `WPARAM`, `VK_*` 등에 의존하지 않는다.
- 입력 상태 조회와 발생 순서가 필요한 이벤트 처리를 모두 지원한다.
- 한 프레임 동안 수집한 입력은 변경되지 않는 스냅샷으로 소비자에게 전달한다.
- OS 메시지 프로토콜을 해석하는 `switch`는 유지하되, 단순 데이터 매핑에는 조회 테이블이나 계산식을 사용한다.
- ImGui가 Win32 메시지를 처리했는지와 관계없이 엔진의 저수준 입력 상태는 항상 수집한다.
- UI 라우팅은 스냅샷 전체가 아니라 이벤트 단위로 처리한다.
- 입력 수집기는 UI, 뷰포트, 게임플레이 정책을 알지 않는다.
- 현재 엔진은 Windows 전용이므로 별도의 범용 `ApplicationCore` 모듈은 만들지 않는다.

## 디렉터리와 의존성

현재 파일 배치는 다음 책임을 따른다.

```text
KnotEngine/Source/Engine/
├─ Input/
│  ├─ InputKeys.h
│  ├─ InputEvents.h
│  ├─ InputSnapshot.h
│  └─ InputSnapshot.cpp
└─ Runtime/
   ├─ WindowsApplication.h/.cpp
   ├─ WindowsWindow.h/.cpp
   └─ WindowsInput.h/.cpp
```

`Input`은 플랫폼에 독립적인 데이터 계층이다. `Runtime`의 Windows 클래스는 OS 메시지를 받고 `Input` 계층의 타입을 생성한다.

```text
Runtime/WindowsInput
        ↓
Input/InputKeys + Input/InputSnapshot
        ↓
Slate / Viewport / Gameplay
```

`FWindowsInput`을 `Input` 폴더에 넣으면 범용 입력 계층에 `Windows.h`와 Win32 메시지 타입이 유입되므로 현재처럼 `Runtime`에 둔다. 플랫폼 계층이 커질 경우에는 `Platform/Windows` 아래로 `WindowsApplication`, `WindowsWindow`, `WindowsInput`을 함께 옮길 수 있지만 현재 규모에서는 별도 계층을 만들지 않는다.

## 현재 구현 상태

### 전체 흐름

```text
Win32 Message Queue
        ↓
FWindowsApplication::WindowProc
        ↓
FWindowsApplication::ProcessMessage
        ├─ FWindowsInput::ProcessMessage
        ├─ ImGui_ImplWin32_WndProcHandler
        └─ 창 수명 및 크기 메시지 처리
        ↓
FWindowsApplication::PumpMessages
        ↓ 모든 메시지를 처리한 뒤
FWindowsInput::TakeSnapshot
        ↓
FInputSnapshot
        ↓
FEngineLoop::Run
        ↓
UEngine::ProcessInput
```

현재 `UEngine::ProcessInput()`은 기본 빈 구현이며 이를 재정의한 엔진 클래스가 없다. 따라서 입력 데이터는 엔진 경계까지 전달되지만 Slate나 뷰포트에서 아직 소비되지 않는다.

### 프레임 실행 순서

1. `FEngineLoop`가 프레임 시간을 계산한다.
2. `FWindowsApplication::PumpMessages()`가 현재 메시지 큐를 모두 비운다.
3. 각 메시지는 `FWindowsInput`에 즉시 누적된다.
4. 메시지 처리가 끝나면 `TakeSnapshot()`을 정확히 한 번 호출한다.
5. 생성된 스냅샷을 `GEngine->ProcessInput()`에 전달한다.
6. 입력 처리 이후 `GEngine->Tick()`을 실행한다.

이 순서 때문에 같은 프레임의 Tick은 해당 프레임 메시지 큐를 모두 반영한 입력 상태를 본다.

## 계층별 책임

| 계층 | 현재 책임 | 책임에 포함하지 않는 것 |
|---|---|---|
| `FWindowsApplication` | 윈도우 생성, 메시지 펌프, `WindowProc`, 창 수명과 리사이즈, 스냅샷 보관 | 키 상태 계산, UI 포커스 정책 |
| `FWindowsInput` | Win32 입력 해석, 키 변환, 상태 누적, 순서 보존 이벤트 생성, Raw Mouse 수집, 스냅샷 생성 | ImGui/Slate 정책, 뷰포트 선택, 액션 매핑 |
| `FInputSnapshot` | 한 프레임 입력 상태와 이벤트의 읽기 전용 조회 인터페이스 | 상태 변경, OS 호출, 소비 대상 선택 |
| `FSlateApplication` | 아직 미구현. 위젯 포커스·캡처·호버와 이벤트별 라우팅 담당 예정 | Win32 키 코드 해석 |
| `FSceneViewport` | 아직 미구현. Slate의 Viewport 위젯과 `FViewportClient` 연결 예정 | 전역 입력 수집 |
| `FViewportClient` | 현재 Tick만 제공. 에디터 또는 게임 뷰포트 입력 소비 지점으로 확장 예정 | OS 메시지 직접 처리 |

## 입력 데이터 모델

### 엔진 입력 타입

`InputKeys.h`는 키, 버튼 및 비트 마스크를 정의하고 `InputEvents.h`는 순서 보존 이벤트를 정의한다.

| 타입 | 의미 |
|---|---|
| `EKeyboardKey` | 엔진에서 사용하는 키 식별자 |
| `EMouseButton` | Left, Right, Middle, Thumb1, Thumb2 |
| `EMouseButtonMask` | 동시에 눌린 마우스 버튼 비트 집합 |
| `EModifierKeyMask` | Shift, Control, Alt, Super 비트 집합 |
| `FKeyInputEvent` | 키와 `bDown`, modifier, 반복 여부 |
| `FPointerInputEvent` | 이동, Raw 이동, 버튼 Down/Up과 더블 클릭 여부, 휠 |
| `FCharacterInputEvent` | UTF-32 문자 입력 |
| `FFocusInputEvent` | 네이티브 윈도우 포커스 변경 |

`FInputEvent`는 이 이벤트 구조체를 담는 `std::variant`다. 소비 계층에서는 이벤트 종류를 별도 정수 코드로 재해석하기보다 `std::visit`을 사용해 타입별로 처리할 수 있다.

### 상태와 이벤트를 함께 보관하는 이유

스냅샷에는 현재 상태와 순서 보존 이벤트 배열이 모두 있다.

- 상태 조회는 카메라 이동처럼 매 프레임 계속 확인하는 입력에 사용한다.
- 이벤트 배열은 텍스트 입력, 더블 클릭, 한 프레임 안의 Down/Up 순서처럼 상태만으로 복원할 수 없는 정보에 사용한다.

예를 들어 같은 프레임 안에 키를 눌렀다가 떼면 최종 `KeysDown`은 `false`지만 `KeysPressed`, `KeysReleased`와 이벤트 배열에는 두 전환이 모두 남는다.

## FInputSnapshot

`FInputSnapshot`은 외부에 const 조회 함수만 제공하고 내부 필드는 `FWindowsInput`만 채울 수 있다.

주요 조회 API는 다음과 같다.

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

### 데이터 수명

| 데이터 | 스냅샷 생성 후 수집기 상태 |
|---|---|
| `KeysDown` | 다음 프레임까지 유지 |
| `KeysPressed` | `false`로 초기화 |
| `KeysReleased` | `false`로 초기화 |
| `MouseButtonsDown` | 다음 프레임까지 유지 |
| `MouseButtonsPressed` | `None`으로 초기화 |
| `MouseButtonsReleased` | `None`으로 초기화 |
| `PointerPosition` | 마지막 클라이언트 위치 유지 |
| `PointerDelta` | Zero로 초기화 |
| `RawPointerDelta` | Zero로 초기화 |
| `WheelDelta` | Zero로 초기화 |
| `PendingEvents` | 비움 |
| 포커스 상태 | 다음 프레임까지 유지 |

`FrameNumber`는 1부터 증가한다. 정수 공간을 모두 사용해 0으로 순환한 뒤 새 스냅샷을 만들려고 하면 `panicf`로 중단한다.

`TakeSnapshot()`은 프레임당 한 번만 호출해야 한다. 한 프레임에 여러 번 호출하면 첫 번째 호출 이후 순간 상태와 이벤트가 초기화되어 뒤 소비자는 빈 전환 상태를 받는다.

## FWindowsInput

### 수명

`FWindowsInput`은 `FWindowsApplication`의 값 멤버이며 수명은 프로세스 범위다. `Startup()`과 `Shutdown()`은 `FEngineLoop::Startup()`과 `FEngineLoop::Shutdown()`에서 각각 한 번만 호출한다. 창을 다시 만드는 경로가 없으므로 같은 인스턴스를 다시 `Startup()`하는 재시작은 지원하지 않는다.

`Shutdown()`은 Raw Input 등록을 해제하고 `WindowHandle`과 `PendingEvents`만 정리한다. 눌린 키와 마우스 버튼 상태는 초기화하지 않으므로 재시작하면 이전 상태를 물려받는다. 재시작이 필요해지면 그 시점의 요구사항에 맞춰 초기화 범위와 `NextFrameNumber` 처리 규칙을 함께 정의해야 한다.

### 메시지 분배

`ProcessMessage()`의 `switch`는 Win32 메시지 프로토콜을 해석하는 시스템 경계이므로 유지한다. 각 case는 다음 private 처리 함수에 작업을 위임하며 메시지 분배 함수 자체에는 상태 변경 세부 구현을 넣지 않는다.

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

단순 데이터 매핑은 `switch`로 작성하지 않는다.

- Win32 Virtual Key 변환은 256칸 `constexpr` 조회 테이블을 사용한다.
- 숫자, 알파벳, NumPad 숫자, F1-F12는 연속 범위를 계산해 테이블을 채운다.
- 마우스 버튼 마스크는 버튼 인덱스에 대한 비트 시프트로 계산한다.

### 키 변환

`TranslateVirtualKey()`는 Win32 키를 `EKeyboardKey`로 변환한다.

- Shift는 스캔 코드와 `MapVirtualKeyW()`를 사용해 좌우를 구분한다.
- Control과 Alt는 extended-key 비트로 좌우를 구분한다.
- extended Enter는 `NumPadEnter`로 구분한다.
- NumLock이 꺼진 NumPad 숫자 키는 extended-key 비트가 없는 탐색 키 메시지를 NumPad 키로 복원한다.
- 등록하지 않은 Virtual Key는 `EKeyboardKey::Unknown`으로 변환한다.
- `Unknown`은 키 상태와 이벤트에 기록하지 않는다.

연속 범위 변환은 `EKeyboardKey` 열거형 배치에 의존하므로 숫자, 알파벳, NumPad, Function Key의 끝 값에 `static_assert`를 둔다. 이 범위 사이에 새 열거형 값을 삽입하면 컴파일 단계에서 매핑 오류를 발견해야 한다.

### 키 상태와 반복 입력

최초 Key Down은 `KeysDown`과 `KeysPressed`를 설정하고 Down 이벤트를 생성한다. 이미 눌린 키에서 반복 메시지가 들어오면 `KeysPressed`를 다시 설정하지 않고 `bRepeat = true`인 Down 이벤트만 생성한다.

Key Up은 `KeysDown`을 해제하고 `KeysReleased`와 Up 이벤트를 기록한다. 실제 상태 변화가 없는 중복 Up은 무시한다.

### 포커스

`WM_SETFOCUS`와 `WM_KILLFOCUS`는 `FFocusInputEvent`를 만든다. 포커스를 잃으면 다음 상태를 정리한다.

- 눌린 모든 키를 해제하고 `KeysReleased`에 기록한다.
- 눌린 모든 마우스 버튼을 해제하고 `MouseButtonsReleased`에 기록한다.
- 포인터 델타와 Raw 델타를 초기화한다.
- 마지막 포인터 위치를 유효하지 않은 상태로 만든다.
- 조합 대기 중인 UTF-16 상위 서로게이트를 폐기한다.

포커스 손실에 의한 일괄 해제는 현재 개별 Key Up 및 Button Up 이벤트를 만들지 않고 release 상태와 Focus 이벤트로 표현한다. 향후 이벤트 소비자가 포커스 손실 시 개별 release 이벤트를 요구하는지 결정해야 한다.

### 마우스 버튼과 캡처

버튼을 누르면 해당 시점의 포인터 위치를 반영하고 `SetCapture()`를 호출한다. 버튼을 뗀 뒤 눌린 버튼이 하나도 남지 않았을 때만 `ReleaseCapture()`를 호출한다. 따라서 여러 버튼을 동시에 누른 상태에서 하나만 떼어도 캡처가 유지된다.

`WM_CAPTURECHANGED`로 다른 윈도우에 캡처를 빼앗기면 눌린 버튼을 모두 해제하고 Button Up 이벤트를 생성한다.

더블 클릭의 두 번째 누름도 `ButtonDown` 이벤트로 전달하며 `bDoubleClick`만 `true`로 설정한다. 따라서 이벤트 순서는 `ButtonDown → ButtonUp → ButtonDown(bDoubleClick) → ButtonUp`으로 유지된다.

### 포인터 좌표와 두 종류의 델타

`PointerDelta`와 `RawPointerDelta`는 같은 데이터를 중복 보관하는 값이 아니다.

| 값 | 출처 | 단위와 특성 | 주 용도 |
|---|---|---|---|
| `PointerDelta` | `WM_MOUSEMOVE`의 현재/이전 클라이언트 좌표 차이 | 클라이언트 픽셀, Windows 커서 설정 반영, 화면 경계 영향 | UI 호버와 드래그 |
| `RawPointerDelta` | `WM_INPUT`의 `lLastX`, `lLastY` | 장치 상대 이동 count, 커서 위치와 독립적 | 에디터/FPS 카메라 회전 |

`PointerDelta`는 한 프레임에 발생한 위치 변화의 합이다. 커서가 화면 경계에 도달하면 물리적으로 마우스를 계속 움직여도 값이 증가하지 않을 수 있다.

`RawPointerDelta`는 상대 이동 Raw Mouse 입력의 합이다. 커서를 숨기거나 중앙에 재배치하는 카메라 모드에서도 사용할 수 있다. 현재 구현은 `RIM_TYPEMOUSE`의 상대 입력만 처리하고 `MOUSE_MOVE_ABSOLUTE` 장치는 무시한다.

`RawPointerDelta`의 현재 생산자는 마우스뿐이므로 의미상 `RawMouseDelta`가 더 구체적인 이름일 수 있지만, 문서와 API는 현행 이름을 기준으로 한다.

### 휠

세로 휠과 가로 휠을 모두 `FVector2`로 표현한다.

- 세로 휠: `(0, Delta)`
- 가로 휠: `(-Delta, 0)`

휠 메시지의 좌표는 화면 좌표이므로 `ScreenToClient()`를 거쳐 스냅샷의 포인터 위치와 같은 클라이언트 좌표계로 변환한다.

### 문자 입력

키 이벤트와 문자 이벤트는 분리한다. 텍스트 입력은 키보드 배열을 문자로 재해석하지 않고 `WM_CHAR`와 `WM_UNICHAR`에서 생성한다. Alt 조합으로 발생하는 `WM_SYSCHAR`는 시스템 메뉴 처리를 위해 `DefWindowProc()`에 맡긴다.

U+FFFF를 넘는 문자는 UTF-16 서로게이트 쌍으로 표현되어 `WM_CHAR` 두 번에 나뉘어 도착한다. 상위 서로게이트를 보관했다가 하위 서로게이트가 오면 하나의 UTF-32 코드 포인트로 조합한다. 유효한 쌍을 만들지 못한 서로게이트는 버린다. 문자 메시지의 하위 16비트 반복 횟수만큼 문자 이벤트를 생성한다.

## ImGui와의 관계

현재 `FWindowsApplication::ProcessMessage()`는 다음 순서로 동작한다.

```text
FWindowsInput::ProcessMessage
        ↓
ImGui_ImplWin32_WndProcHandler
        ↓
FWindowsApplication의 창 메시지 처리
```

엔진 입력을 ImGui보다 먼저 수집하므로 ImGui가 메시지를 처리해도 held/pressed/released 상태가 누락되지 않는다. ImGui의 소비 여부는 저수준 입력 수집을 막는 조건이 아니다.

향후 `FSlateApplication`은 ImGui 타입이나 `ImGuiIO`에 의존하지 않는다. ImGui를 계속 사용하는 동안에는 Win32 백엔드가 병행할 수 있지만, Slate 입력 포커스와 캡처 정책의 기준은 Slate 자체 상태여야 한다.

## 목표 Slate 입력 구조

별도의 전역 `FInputRouter`를 추가하지 않고 `FSlateApplication`이 UI 입력 라우터 역할을 맡는다.

```text
FWindowsApplication
        ↓
FWindowsInput
        ↓
FInputSnapshot
        ↓
FSlateApplication::ProcessInput
        ├─ Mouse Captured Widget
        ├─ Keyboard Focused Widget
        ├─ Hovered Widget
        └─ SViewport
              ↓
        FSceneViewport
              ↓
        FViewportClient
              ├─ Editor Viewport Camera
              └─ GameViewportClient
                    ↓
                 LocalPlayer
                    ↓
              PlayerController
                    ↓
          PlayerInput / Enhanced Input
                    ↓
             InputComponent / Delegate
```

### SlateApplication의 책임

- 위젯 트리 hit test
- 키보드 포커스 소유자 관리
- 마우스 캡처 소유자 관리
- 호버 경로 관리
- `Snapshot.Events`의 발생 순서 보존
- 이벤트별 target 결정
- 이벤트별 handled/unhandled 결과 관리
- 처리되지 않은 Viewport 입력을 `FSceneViewport`로 전달

### 이벤트 단위 라우팅

스냅샷 전체를 하나의 `bool`로 소비하면 안 된다. 같은 프레임에 UI 클릭과 게임 키 입력이 함께 발생할 수 있기 때문이다.

```text
Mouse Button Event → Captured/Hovered Widget → handled
Key Event          → Focused SViewport       → unhandled → ViewportClient
Character Event    → Focused Text Widget     → handled
Raw Move Event     → Captured SceneViewport  → Camera
```

라우팅 결과는 이벤트마다 기록해야 한다. Slate가 마우스 이벤트를 소비했다고 해서 같은 스냅샷의 키보드 상태까지 게임플레이에서 차단해서는 안 된다.

지속 입력은 상태 조회를 사용하되, 어느 뷰포트가 해당 상태를 사용할 수 있는지는 Slate의 현재 포커스와 캡처 결과로 제한한다.

### InputRouter를 별도로 두지 않는 이유

캡처, 포커스, 호버, hit test는 위젯 시스템의 상태다. 별도 `InputRouter`와 `FSlateApplication`이 이 상태를 각각 보관하면 소유권이 중복되고 서로 다른 target을 선택할 수 있다.

따라서 다음 기능은 `FSlateApplication`에 통합한다.

- UI 우선순위
- 마우스 캡처
- 키보드 포커스
- 호버 target
- 이벤트 소비 결과
- Viewport 브리지 선택

## ApplicationCore에 대한 결정

Knot Engine은 Windows 전용이며 다른 데스크톱 또는 콘솔 플랫폼 확장을 목표로 하지 않는다. 그러므로 Unreal Engine처럼 별도 `ApplicationCore` 모듈과 Generic Application 계층을 복제하지 않는다.

현재 구조에서 역할은 다음과 같이 대응한다.

```text
OS/Application 계층  = FWindowsApplication + FWindowsWindow + FWindowsInput
UI Application 계층  = FSlateApplication (예정)
범용 입력 데이터     = InputTypes + FInputSnapshot
```

플랫폼 비종속성을 유지해야 하는 경계는 전체 Application 구현이 아니라 `FInputSnapshot` 이후다.

## LunaticEngine 참고 결과

LunaticEngine의 `InputManager → InputSystemSnapshot → InputRouter → ViewportClient` 구조에서 다음 개념을 참고한다.

- Win32 메시지를 프레임 이벤트로 누적하는 경계
- 프레임 스냅샷 전달
- 캡처, 포커스, 호버 target 구분
- Enhanced Input을 OS 입력 수집기와 분리하는 방향

다음 구현 방식은 Knot Engine에 가져오지 않는다.

- 입력 관리자의 전역 싱글턴 접근
- 엔진 전역에 Win32 `VK_*` 값을 노출하는 방식
- 입력 관리자 내부의 ImGui 의존성
- 플랫폼 계층에서 드래그와 UI 정책까지 계산하는 방식
- 전체 스냅샷을 단일 `bool`로 소비하는 라우팅
- UI와 컴포넌트가 입력 관리자를 직접 조회하는 우회 경로

Knot Engine은 typed key, typed event, snapshot ownership은 현재 구조를 유지하고, LunaticEngine의 target 관리 아이디어만 Slate에 통합한다.

## 스레딩과 소유권

현재 입력 수집과 소비는 모두 메인 스레드에서 수행한다.

- `WindowProc`와 메시지 펌프가 `FWindowsInput` 상태를 변경한다.
- `TakeSnapshot()`이 같은 스레드에서 해당 상태를 복사한다.
- `FEngineLoop`가 같은 스레드에서 스냅샷을 전달한다.
- 현재 구현에는 동기화 primitive가 없으며 멀티스레드 접근을 지원하지 않는다.

렌더 스레드나 작업 스레드는 `FWindowsInput`을 직접 참조하지 않는다. 향후 다른 스레드가 입력을 필요로 하면 완성된 스냅샷의 명시적인 복사본을 전달해야 한다.

## 현재 제약 및 남은 작업

### 미구현 소비 계층

- `UEngine::ProcessInput()`의 Editor/Game 구현
- ImGui에 의존하지 않는 `FSlateApplication`
- 기본 Widget 이벤트와 handled 결과
- 키보드 포커스, 마우스 캡처, hover path
- `SViewport`와 `FSceneViewport`
- `FViewportClient` 입력 인터페이스
- Editor Viewport 카메라 입력 연결
- GameViewportClient 이후 PlayerController/Enhanced Input 연결

### 현재 지원 범위

- 단일 네이티브 윈도우
- 키보드
- UTF 문자 입력
- 일반 마우스 버튼과 Thumb 버튼
- 세로/가로 휠
- 상대 Raw Mouse 이동
- 포커스와 포인터 캡처

현재 지원하지 않는 범위는 다중 윈도우별 입력 context, 게임패드, 터치, 펜, IME composition, 입력 재매핑과 액션 시스템이다.

## 확장 규칙

### 키를 추가할 때

1. `EKeyboardKey`에 엔진 키를 추가한다.
2. `TranslateVirtualKey()`의 정적 테이블에 Win32 매핑을 추가한다.
3. 연속 키 범위 안에 값을 삽입했다면 관련 `static_assert`와 범위 매핑을 확인한다.
4. 지원하지 않는 키는 계속 `Unknown`으로 남겨야 한다.

### 이벤트 종류를 추가할 때

1. 의미가 분명한 event struct를 추가한다.
2. `FInputEvent` variant에 타입을 등록한다.
3. `FWindowsInput`에서 발생 순서대로 `PendingEvents`에 추가한다.
4. Slate 라우팅에서 해당 타입의 target과 handled 규칙을 정의한다.
5. 단순 상태 조회만으로 충분한 값이라면 새 이벤트를 만들 필요가 있는지 먼저 검토한다.

### 새 입력 장치를 추가할 때

장치별 OS 해석은 Windows Runtime 계층에 둔다. 엔진 소비자가 Win32 handle이나 장치 구조체를 받지 않도록 플랫폼 독립 이벤트 또는 상태로 변환한 뒤 스냅샷에 합친다.

## 검증 항목

입력 계층을 변경할 때 다음 시나리오를 확인한다.

- 키를 누른 프레임에 `IsKeyDown`과 `WasKeyPressed`가 모두 true다.
- 키를 유지한 다음 프레임에는 `IsKeyDown`만 true다.
- 키를 뗀 프레임에는 `WasKeyReleased`가 true다.
- 반복 Key Down은 `WasKeyPressed`를 반복 생성하지 않고 repeat 이벤트만 만든다.
- 같은 프레임의 Key Down/Up 이벤트 순서가 보존된다.
- 좌우 Shift, Control, Alt가 구분된다.
- 일반 Enter와 NumPad Enter가 구분된다.
- NumLock이 꺼져도 NumPad 숫자 키와 전용 탐색 키가 구분된다.
- 문자 메시지의 반복 횟수만큼 문자 이벤트가 생성된다.
- `WM_SYSCHAR`는 일반 문자 이벤트를 생성하지 않는다.
- 두 조각으로 쪼개져 오는 UTF-16 문자가 하나의 UTF-32 문자로 변환된다.
- 더블 클릭이 `ButtonDown → ButtonUp → ButtonDown(bDoubleClick) → ButtonUp` 순서로 기록된다.
- 여러 마우스 버튼 중 하나만 떼면 포인터 캡처가 유지된다.
- 캡처를 빼앗기면 모든 눌린 마우스 버튼이 해제된다.
- 포커스를 잃으면 held key와 mouse button이 남지 않는다.
- `PointerDelta`는 클라이언트 픽셀 이동을 나타낸다.
- `RawPointerDelta`는 화면 경계에서도 상대 이동을 유지한다.
- 세로/가로 휠이 서로 다른 축에 누적된다.
- ImGui가 메시지를 처리해도 엔진 스냅샷의 입력 상태가 누락되지 않는다.
- `TakeSnapshot()` 이후 순간 상태와 이벤트만 초기화되고 held 상태는 유지된다.

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
- [ViewportClient.h](../KnotEngine/Source/Editor/Viewport/ViewportClient.h)
- [Conventions.md](../Conventions.md)
