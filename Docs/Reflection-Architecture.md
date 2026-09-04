# Knot Engine Reflection Architecture

## 문서 목적

이 문서는 Knot Engine의 리플렉션 스키마가 C++ 선언에서 생성되고, 런타임에 등록·조회·사용·해제되는 흐름을 정의한다.

리플렉션은 타입과 멤버를 설명하는 데 그치지 않는다. 같은 스키마를 에디터 프로퍼티 열거, 값 접근, 객체와 구조체 생성, 함수 호출, 직렬화, 강한 참조 수집에 공통으로 사용한다. 현재 범위에는 동적 클래스, CDO, hot reload, replication, 스크립팅 VM과 에디터 property customization이 포함되지 않는다.

## 설계 원칙

- C++ 선언을 단일 원본으로 사용하고 생성 코드를 직접 수정하지 않는다.
- `UObject` 인스턴스와 이를 설명하는 `UClass` 스키마를 구분한다.
- C++ 타입의 정적 클래스와 객체의 실제 런타임 클래스를 별도로 저장한다.
- `FReflectionRegistry`가 최상위 스키마를, `UStruct`와 `UClass`가 멤버 스키마를 소유한다.
- `FProperty`는 값을 소유하지 않고 오프셋과 타입별 연산만 제공한다.
- 에디터 열거, 직렬화와 참조 수집은 같은 프로퍼티 스키마를 각자의 플래그 정책으로 순회한다.
- `TObjectPtr`는 강한 참조로, `TSoftObjectPtr`는 물리 에셋 경로 기반 비소유 참조로 취급한다.
- 등록과 객체 생명주기 변경은 메인 스레드에서 수행한다.
- 현재 필요한 구체 타입과 연산만 제공하며 동적 타입 시스템이나 별도 범용 필드 계층을 미리 만들지 않는다.

## 전체 구조

리플렉션은 빌드 시점의 스키마 생성과 런타임의 스키마 사용으로 나뉜다.

```text
C++ Header
    ├─ UCLASS / USTRUCT / UENUM
    └─ UPROPERTY / UFUNCTION
            ↓
KnotHeaderTool.py
    ├─ Clang AST로 선언과 실제 타입 해석
    ├─ 전처리 기록과 토큰으로 마커 해석
    └─ 옵션, 상속, 지원 타입 검증
            ↓
Reflection.gen.cpp
    ├─ 타입 스키마 생성
    ├─ 프로퍼티와 함수 연결
    └─ native 함수 invoker 생성
            ↓
FReflectionRegistry::Startup
            ↓
런타임 조회 / 에디터 / 호출 / 직렬화 / 참조 수집
```

런타임 스키마는 두 계층으로 구성한다.

```text
UObject
└─ UField
   ├─ UEnum
   └─ UStruct
      ├─ UClass
      ├─ UScriptStruct
      └─ UFunction

FProperty
├─ TNumericProperty<T>
│  ├─ FIntProperty
│  ├─ FBoolProperty
│  ├─ FFloatProperty
│  └─ FDoubleProperty
├─ FStringProperty
├─ FNameProperty
├─ FEnumProperty
├─ FStructProperty
├─ FObjectProperty
├─ FSoftObjectProperty
└─ FArrayProperty
```

`UField` 계층은 Registry에 등록되는 UObject 기반 스키마다. `FProperty` 계층은 컨테이너 안의 값에 접근하는 경량 메타데이터이며 UObject를 상속하지 않는다.

## 디렉터리와 책임

```text
KnotEngine/Source/Engine/Object/
├─ Object.h/.cpp
├─ ObjectPtr.h/.cpp
├─ Class.h/.cpp
├─ Function.h/.cpp
├─ Property.h/.cpp
├─ ReferenceCollector.h/.cpp
├─ Property/
│  ├─ NumericProperty.h
│  ├─ StringProperty.h/.cpp
│  ├─ NameProperty.h/.cpp
│  ├─ EnumProperty.h/.cpp
│  ├─ StructProperty.h/.cpp
│  ├─ ObjectProperty.h/.cpp
│  ├─ SoftObjectProperty.h/.cpp
│  └─ ArrayProperty.h/.cpp
└─ Reflection/
   ├─ ReflectionMacros.h
   ├─ ReflectionMetadata.h
   └─ ReflectionRegistry.h/.cpp

Scripts/
├─ KnotHeaderTool.py
└─ Toolchain.py

KnotEngine/Build/CMake/
└─ Reflection.cmake
```

| 타입 또는 파일 | 책임 |
|---|---|
| `UObject` | UUID, 전역 객체 배열 등록과 실제 런타임 클래스 보관 |
| `UField` | 스키마 이름, 소유자와 메타데이터 |
| `UStruct` | 크기, 정렬, 상속 관계와 프로퍼티 소유 |
| `UClass` | 클래스 플래그, 객체 생성 함수와 함수 스키마 소유 |
| `UScriptStruct` | 값 타입의 생성, 소멸과 복사 |
| `UFunction` | 매개변수 스키마와 native 호출 |
| `FProperty` | 오프셋, 크기, 플래그와 타입별 값 연산 |
| `FReflectionRegistry` | 최상위 스키마 소유, `FName` 기반 조회와 코어 타입 등록 |
| `FReferenceCollector` | 강한 객체 참조의 도달 가능 집합 계산 |
| `KnotHeaderTool.py` | 마커가 붙은 C++ 선언 검증과 코드 생성 |
| `Reflection.cmake` | 생성기를 빌드의 `PRE_BUILD` 단계에 연결 |

## 선언과 조회

### 기본 선언

헤더에 마커를 직접 작성한다. 마커와 멤버는 같은 줄이나 별도 줄에 둘 수 있으며, 멤버마다 마커 하나를 사용한다. 사용자 헤더에 generated header를 포함할 필요는 없다.

```cpp
#include "Object/Object.h"
#include "Object/Reflection/ReflectionMacros.h"
#include "Core/Math/Vector2.h"

UCLASS()
class UExample : public UObject
{
    GENERATED_CLASS(UExample, UObject)

private:
    /// 텍스처 좌표입니다.
    UPROPERTY(Category = "Rendering") FVector2 UV;
    UPROPERTY(NoEdit, Transient) TObjectPtr<UObject> Target;
    UPROPERTY() bool bVisible = true;

public:
    UFUNCTION()
    void SetVisible(bool bInVisible) { bVisible = bInVisible; }
};
```

- UObject 파생 타입은 `UCLASS()`와 `GENERATED_CLASS(Type, Parent)`를 사용한다.
- 값 타입은 `USTRUCT()`와 `GENERATED_STRUCT(Type)`를 사용한다.
- 열거형은 `UENUM()`을 사용하며 별도의 generated body 마커가 없다.
- `UPROPERTY`가 붙은 필드만 등록한다. C++ 접근 지정자와 관계없이 마커 없는 필드는 제외한다.
- `UFUNCTION`이 붙은 UObject 멤버 함수만 함수 스키마와 native invoker를 생성한다.

`GENERATED_CLASS`는 `ThisClass`, `Super`, `StaticClass()`와 생성 코드의 private 접근 권한을 선언한다. `ThisClass`는 파생 타입이 마커를 빠뜨려 부모 스키마로 생성되는 실수를 컴파일 단계에서 막는다. `GENERATED_STRUCT`는 `StaticStruct()`와 생성 코드의 접근 권한을 선언한다. 두 매크로의 정적 포인터 정의는 생성된 `.cpp`에 둔다.

### 타입 조회

| 대상 | 조회 방법 |
|---|---|
| UObject C++ 타입 | `Type::StaticClass()` |
| UObject 인스턴스의 실제 타입 | `Object->GetClass()` |
| 구조체 타입 | `Type::StaticStruct()` |
| 열거형 타입 | `StaticEnum<Type>()` |
| 이름 기반 최상위 스키마 | `GReflectionRegistry->FindField(FName(...))` |

`StaticEnum<T>()` 선언은 `ReflectionRegistry.h`에 있고 enum별 특수화는 생성된 `.cpp`에 있다. 각 정적 조회 함수는 생성 코드가 연결한 포인터를 직접 반환하므로 C++ `type_index` 검색 맵은 사용하지 않는다.

### 옵션과 툴팁

| `UPROPERTY` 선언 | 에디터 열거 | 직렬화 | 강한 참조 수집 |
|---|---|---|---|
| `UPROPERTY()` | 포함 | 포함 | 포함 |
| `UPROPERTY(NoEdit)` | 제외 | 포함 | 포함 |
| `UPROPERTY(Transient)` | 포함 | 제외 | 포함 |
| `UPROPERTY(NoEdit, Transient)` | 제외 | 제외 | 포함 |

`NoEdit`은 C++ 접근 제어가 아니라 `GetEditorProperties()`의 필터다. `Transient`도 참조 수집에는 영향을 주지 않는다.

`Category = "Movement"`, `DisplayName = "Speed"`는 모든 등록 마커에서 사용할 수 있다. 기본 카테고리는 선언 타입 이름이다. 표시 이름을 생략하면 `MaxSpeed`는 `Max Speed`로, bool의 `bVisible`은 `Visible`로 변환한다. `Edit`, `Editable`, `EditorOnly`, `Tooltip`, `SaveGame` 등 현재 계약에 없는 옵션은 파서가 거부한다.

툴팁은 마커에 인접한 `///` 또는 `/** */` 문서 주석의 첫 문단에서 가져온다.

- `///` 주석은 마커 바로 위에 연속해서 작성한다.
- `/** */` 주석은 마커 바로 위 또는 마커와 같은 줄에 작성할 수 있다.
- 블록 주석 구분자와 각 줄 앞의 `*`는 제거하고 본문의 줄바꿈은 보존한다.
- 빈 주석 줄은 첫 문단의 끝으로 취급한다.
- 일반 `//`, `/* */`, `////`, `/*** */`와 마커 사이에 빈 소스 줄이 있는 주석은 사용하지 않는다.

```cpp
/** 텍스처 좌표입니다. */
UPROPERTY() FVector2 UV;

/**
 * 이동 속도입니다.
 * 단위는 cm/s입니다.
 */
UPROPERTY() float Speed = 600.0f;
```

### 구조체와 익명 union

```cpp
USTRUCT()
struct FCoordinates
{
    GENERATED_STRUCT(FCoordinates)

public:
    union
    {
        struct
        {
            UPROPERTY() float X;
            UPROPERTY() float Y;
        };
        float Data[2];
    };

    FCoordinates() : X(0.0f), Y(0.0f) {}
};
```

생성기는 익명 struct·union 내부의 표시된 필드를 바깥 타입의 멤버로 등록한다. 위 예제에서는 `X`, `Y`만 등록하고 `Data`는 제외한다. 같은 union의 서로 다른 저장 분기를 동시에 표시하면 생성 오류다. 이름 있는 union 필드를 일반 프로퍼티로 등록하는 기능은 제공하지 않는다.

`FVector2`, `FVector`, `FVector4`, `FQuat`, `FRotator`, `FTransform`도 같은 `USTRUCT` 경로로 등록한다. 별도 내장 수학 타입 목록은 없으므로 일반 구조체와 마찬가지로 단일 값과 배열 원소에 사용할 수 있다.

## 생성기와 빌드

[KnotHeaderTool.py](../Scripts/KnotHeaderTool.py)는 고정된 Clang 20.1.8 libclang과 Python 바인딩을 사용한다. C++ 선언·상속·타입은 AST로, 활성 마커와 옵션은 전처리 기록과 토큰으로 읽는다. 비활성 `#if` 영역은 제외하고 `using`과 `typedef`는 실제 타입으로 해석한다. 선언을 감싸는 사용자 매크로는 지원 계약에 포함하지 않는다.

생성은 다음 순서로 진행한다.

1. 헤더에서 타입과 마커를 수집하고 이름, 상속과 `GENERATED_*` 선언을 검사한다.
2. 표시된 필드와 함수를 연결하고 지원 타입, 옵션과 union 저장 분기를 검사한다.
3. Clang으로 클래스의 public 기본 생성 가능성을 평가한다.
4. 타입 등록, 멤버 연결, 함수 호출과 정적 포인터 해제 코드를 생성한다.

`UPROPERTY() float X, Y;`처럼 한 선언에 여러 필드를 묶는 것은 허용하지 않는다. 잘못된 옵션, 미지원 타입과 연결되지 않은 마커는 원본 파일과 줄 위치를 포함한 오류를 낸다.

[Reflection.cmake](../KnotEngine/Build/CMake/Reflection.cmake)는 MSVC/SDK, include·define, PCH와 구성별 플래그를 생성기에 전달한다. `KnotEngine`의 `PRE_BUILD`에서 입력을 확인하고 `Intermediate/Reflection/<구성>/Reflection.gen.cpp`를 생성한다. 별도 솔루션 프로젝트는 추가하지 않으며 크기, 정렬과 오프셋은 생성된 C++의 `sizeof`, `alignof`, `offsetof`로 계산한다.

[Toolchain.py](../Scripts/Toolchain.py)는 LLVM 배포본의 해시를 검증하고 필요한 도구만 `Intermediate`에 추출해 재사용한다. 매 빌드에서 헤더, 전이 include, 도구, 환경과 산출물의 해시를 확인한다. 변경 없는 파일은 다시 쓰지 않으며 생성 실패 시 빌드를 중단한다.

```powershell
# 저장소 루트에서 프로젝트 생성 및 모든 구성 빌드
python Scripts/GenerateProjects.py --no-open --build-all
```

Visual Studio에서 직접 빌드해도 같은 생성 단계가 실행된다. 새 헤더가 빌드 입력에서 누락되면 프로젝트를 재생성하고, 생성 오류는 출력된 원본 선언 위치에서 수정한다. 대상 환경은 Windows x64 MSVC이며 컴파일러별로 리플렉션 선언을 다르게 만드는 코드는 지원하지 않는다.

## 런타임 등록과 수명

### Registry 소유권

```text
FReflectionRegistry
├─ 최상위 UClass
│  ├─ 직접 선언된 FProperty
│  └─ 직접 선언된 UFunction
├─ 최상위 UScriptStruct
│  └─ 직접 선언된 FProperty
└─ 최상위 UEnum
```

Registry는 최상위 `UClass`, `UScriptStruct`, `UEnum`을 소유한다. `UStruct`는 직접 선언된 프로퍼티를, `UClass`는 직접 선언된 함수를 소유한다. 상속된 멤버는 별도로 복제하지 않고 조회와 열거 시 부모부터 순회한다. 스키마 주소는 Registry가 종료될 때까지 유효하다.

### 시작과 종료

```text
FName 시작
    ↓
코어 스키마 생성 및 등록
    ↓
생성 타입 전체 등록
    ↓
프로퍼티와 함수 연결
    ↓
GReflectionRegistry 설정
```

코어 스키마는 `UObject`, `UField`, `UStruct`, `UClass`, `UScriptStruct`, `UFunction`, `UEnum` 순으로 기반 관계를 연결한다. 모든 타입을 먼저 등록한 다음 멤버를 연결하므로 서로 참조하는 타입도 처리할 수 있다. `UClass` 스키마의 실제 클래스는 `UClass::StaticClass()`이며 `UClass` 자신은 자기 스키마를 가리킨다.

등록 중에는 `GReflectionRegistry`가 아직 `nullptr`이므로 현재 Registry 인스턴스와 타입별 정적 포인터를 사용한다. 등록이 모두 끝난 뒤 `GReflectionRegistry`를 설정하며, 이 포인터가 존재하면 리플렉션 사용 준비가 끝난 것이다.

종료 시에는 일반 객체를 먼저 파괴하고 다음 순서로 스키마를 정리한다.

```text
GReflectionRegistry 해제
    ↓
생성 타입의 정적 포인터 해제
    ↓
코어 타입의 정적 포인터 해제
    ↓
이름 검색 맵과 스키마 파괴
    ↓
FName 종료
```

### 객체 생성과 식별

일반 객체는 `GUObjectManager.Create<T>(Args...)`로 생성한다. 관리자는 생성자를 실행한 뒤 `T::StaticClass()`를 객체의 실제 클래스로 연결하므로 생성자 안에서 `GetClass()`를 호출하지 않는다. UObject는 값 복사와 이동을 금지한다.

public 기본 생성이 가능하고 추상 클래스가 아닌 타입에만 `UClass::CreateObject()`용 생성 함수를 만든다. 생성자 인자가 필요한 타입도 `GUObjectManager.Create<T>(Args...)`로 만들 수 있지만 스키마를 통한 무인자 생성은 할 수 없다.

모든 UObject는 생성 시 UUID와 `GUObjectArray` 인덱스를 받는다. 배열은 파괴 시 swap-remove를 사용하므로 인덱스는 바뀔 수 있다. UUID와 인덱스 모두 현재 실행 중 객체 조회를 위한 값이며 영속 식별자가 아니다.

### 구조체 수명

등록되는 구조체는 기본 생성, 소멸과 복사 대입이 가능해야 한다. `TStructOps<T>`는 미초기화 메모리의 생성·소멸과 이미 생성된 목적지로의 복사 대입을 담당한다.

일반 C++ 객체의 멤버는 원래 생성자가 초기화하므로 리플렉션이 다시 생성하지 않는다. 함수 매개변수 버퍼처럼 리플렉션이 직접 확보한 원시 메모리에만 구조체 수명 연산을 적용한다.

## 런타임 사용

### 프로퍼티 접근과 에디터 열거

`FProperty`는 소유 컨테이너, 바이트 오프셋, 원소 크기, 배열 차원과 플래그를 보관한다. `ContainerPtrToValuePtr()`로 값 주소를 계산하고 구체 프로퍼티가 초기화, 소멸, 복사, 직렬화와 참조 방문을 수행한다.

`UStruct::GetAllProperties()`는 부모부터 현재 타입까지 선언 순서로 프로퍼티를 반환한다. `GetEditorProperties()`는 같은 목록에서 `NoEdit` 프로퍼티만 제외한다.

### 함수 호출

`UFUNCTION`은 실제 C++ 시그니처에 대응하는 매개변수 구조체와 `void(UObject*, void*)` 형식의 native invoker를 생성한다.

```text
UFunction 조회
    ↓
FScopedFunctionParams로 정렬된 버퍼 생성
    ↓
FProperty로 입력값 기록
    ↓
UFunction::Invoke
    ↓
생성된 native invoker가 실제 C++ 멤버 함수 호출
    ↓
FProperty로 출력·반환값 조회
```

`Invoke()`는 Native/Callable 플래그, 매개변수 버퍼와 대상 객체의 클래스 적합성을 검사한다. 부모 클래스에 등록된 가상 함수의 override도 부모 invoker가 C++ 가상 호출을 수행하므로 실제 override로 연결된다. 인자와 반환값이 모두 없으면 버퍼를 할당하지 않는다.

### 직렬화

`UObject::Serialize()`와 `FStructProperty`는 `UStruct::SerializeProperties()`를 사용한다. 직렬화는 메모리 전체를 복사하지 않고 등록된 필드를 부모부터 재귀적으로 순회하며 `Transient` 프로퍼티를 건너뛴다. 배열은 내부 원소 프로퍼티로 처리한다.

강한 객체 참조는 같은 실행에서 이미 존재하는 객체의 UUID로, 소프트 참조는 물리 에셋 경로로 저장한다. 현재 형식은 필드 순서에 의존하며 버전 관리, 이름 변경 대응과 지연 참조 복원을 제공하지 않는다.

### 참조 수집

`FReferenceCollector`는 루트 객체에서 다음 두 경로를 순서대로 방문한다.

1. 객체가 직접 구현한 `AddReferencedObjects()`의 수동 참조
2. 등록된 프로퍼티 안의 `TObjectPtr`, 중첩 구조체와 배열의 강한 참조

방문 집합으로 순환 참조를 차단한다. 마커 없는 `TObjectPtr`는 자동 방문되지 않으므로 필요하면 `AddReferencedObjects()`에서 수동으로 등록한다. `TSoftObjectPtr`는 강한 참조 수집에서 제외되고 경로와 선택적 캐시만 보관한다.

현재 수집기는 도달 가능한 집합만 계산한다. root set, sweep GC와 포인터 자동 무효화는 제공하지 않는다.

## 지원 범위

| 대상 | 지원 범위 |
|---|---|
| 값 | `int32`, `bool`, `float`, `double`, `FString`, `FName`, 등록된 struct·enum |
| 참조와 배열 | `TObjectPtr<T>`, `TSoftObjectPtr<T>`, 기본 allocator의 `TArray<T>` |
| 타입 선언 | namespace 범위의 이름 있는 구체 타입, public 단일 비가상 상속 |
| 함수 | UObject의 일반 멤버 함수, 값 인자·반환, `const T&` 입력, `T&` 입출력, const 함수 |

지원하지 않는 선언에 마커를 붙이면 생성 오류다.

- static·const·volatile·참조 필드, bit-field, 고정 배열, 일반 포인터, Map, `TArray<bool>`과 사용자 allocator
- 중첩 타입과 익명 namespace의 반영 타입, 다중·가상·비공개 상속, 상속 프로퍼티 이름 가리기, 대소문자만 다른 중복 이름
- static 함수, overload, 함수 템플릿, operator, 가변 인자, rvalue reference, 참조 반환, ref-qualified·volatile 함수

객체 포인터 프로퍼티는 `TObjectPtr`를 사용한다. 런타임에서 수동으로 만든 `FObjectProperty`가 raw pointer 연산을 지원하더라도 자동 생성 파서는 일반 포인터를 허용하지 않는다. 열거값은 런타임 스키마의 `int64` 범위 안에 있어야 한다.

## 스레딩과 수명

객체 생성·파괴, Registry 변경과 참조 수집은 메인 스레드에서 수행한다. 현재 구현에는 Registry나 객체 배열을 보호하는 동기화가 없으므로 다른 스레드에서 이 상태를 변경하지 않는다.

객체는 자신의 스키마보다 먼저 파괴해야 한다. 함수 매개변수 버퍼는 `FScopedFunctionParams` 범위 안에서만 유효하고, 프로퍼티와 함수 스키마 주소는 Registry 종료 전까지만 유효하다.

## 현재 구현 상태

### 구현됨

- 정적 C++ 클래스, 구조체와 열거형 등록
- 프로퍼티와 함수 스키마 자동 생성
- 객체 생성과 구조체 생성·소멸·복사 연산
- 에디터 프로퍼티 열거와 메타데이터
- 등록 필드 기반 직렬화
- 강한 참조의 도달 가능 집합 수집

### 미구현

- 동적 클래스와 동적으로 추가되는 인스턴스 데이터
- CDO와 hot reload
- root set과 sweep GC
- 에셋 로딩과 지연 참조 복원
- 직렬화 버전 관리와 필드 이름 변경 대응
- replication과 스크립팅 VM
- 에디터 property customization

## 관련 파일

- [ReflectionMacros.h](../KnotEngine/Source/Engine/Object/Reflection/ReflectionMacros.h), [ReflectionRegistry.cpp](../KnotEngine/Source/Engine/Object/Reflection/ReflectionRegistry.cpp)
- [Object.h](../KnotEngine/Source/Engine/Object/Object.h), [Class.h](../KnotEngine/Source/Engine/Object/Class.h), [Property.h](../KnotEngine/Source/Engine/Object/Property.h)
- [Function.h](../KnotEngine/Source/Engine/Object/Function.h), [ReferenceCollector.h](../KnotEngine/Source/Engine/Object/ReferenceCollector.h), [ObjectPtr.h](../KnotEngine/Source/Engine/Object/ObjectPtr.h)
- [KnotHeaderTool.py](../Scripts/KnotHeaderTool.py), [Reflection.cmake](../KnotEngine/Build/CMake/Reflection.cmake)
- [Conventions.md](Conventions.md)
