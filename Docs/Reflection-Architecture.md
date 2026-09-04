# Knot Engine Reflection Architecture

## 문서 목적

이 문서는 Knot Engine의 런타임 타입 스키마, 객체 클래스 식별, 프로퍼티, 함수 호출, 참조 수집과 등록소의 책임을 정의한다.

Unreal Engine의 구조를 참고하되 현재 필요하지 않은 CDO, Blueprint VM, replication, package loading과 hot reload는 구현하지 않는다.

## 설계 원칙

- `UObject` 인스턴스와 이를 설명하는 `UClass` 스키마를 구분한다.
- C++ 타입의 정적 클래스와 객체의 실제 런타임 클래스를 별도로 저장한다.
- `FReflectionRegistry`가 최상위 스키마를 소유하고 코어 타입을 먼저 등록한다.
- `UStruct`는 프로퍼티를, `UClass`는 함수 스키마를 직접 소유한다.
- `FProperty`는 값을 소유하지 않고 위치와 타입별 연산만 제공한다.
- `FProperty`는 별도의 `FField` 기반 없이 독립적인 경량 메타 타입으로 둔다.
- raw `UObject*`와 `TObjectPtr<T>`의 저장 방식은 `IObjectPtrOps`로 타입 소거한다.
- `TObjectPtr`는 강한 참조이고 `TSoftObjectPtr`는 물리 에셋 경로 기반 비소유 참조다.
- 일반 코드는 필요한 프로퍼티 헤더만 직접 포함하며 umbrella header를 두지 않는다.
- Lua와 Blueprint callable/pure 플래그 및 실행 경로는 아직 포함하지 않는다.

## 전체 구조

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
  │   ├─ FIntProperty
  │   ├─ FBoolProperty
  │   ├─ FFloatProperty
  │   └─ FDoubleProperty
  ├─ FStringProperty
  ├─ FNameProperty
  ├─ FEnumProperty
  ├─ FStructProperty
  ├─ FObjectProperty
  ├─ FSoftObjectProperty
  └─ FArrayProperty
```

`UField` 계층은 UObject 기반 스키마다. `FProperty` 계층은 값 접근을 위한 경량 메타데이터이며 UObject나 `FField`를 상속하지 않는다. `FMapProperty`와 추가 정수 폭 프로퍼티는 필요할 때 별도로 설계한다.

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
```

| 타입 | 책임 |
|---|---|
| `UObject` | 객체 식별, 실제 런타임 클래스, 전역 객체 배열 등록 |
| `UField` | 스키마 이름, 소유자와 메타데이터 |
| `UStruct` | 크기, 정렬, 상속 관계와 프로퍼티 소유 |
| `UClass` | 클래스 플래그, 객체 생성 함수와 함수 소유 |
| `UScriptStruct` | 값 타입의 생성, 소멸과 복사 |
| `UFunction` | 매개변수 스키마와 native 호출 |
| `FProperty` | 오프셋, 크기, 플래그와 값 연산 |
| `FReflectionRegistry` | 스키마 소유, 이름 검색과 코어 타입 등록 |
| `FReferenceCollector` | 강한 객체 참조의 도달 가능성 계산 |

## UObject 클래스 식별

### 정적 클래스와 실제 클래스

각 UObject 타입은 두 종류의 클래스 포인터를 구분한다.

```cpp
static UClass* StaticClassPrivate;
UClass* ClassPrivate = nullptr;
```

```text
AActor::StaticClassPrivate ─────────▶ UClass("AActor")

PlayerObject.ClassPrivate ──────────▶ UClass("BP_Player")
EnemyObject.ClassPrivate ───────────▶ UClass("BP_Enemy")
```

`StaticClassPrivate`은 C++ 타입 하나에 대응한다. `ClassPrivate`은 객체마다 실제 런타임 클래스를 가리킨다. 여러 객체가 하나의 `UClass`를 공유하며 객체가 `UClass`를 개별 소유하지 않는다.

`GetClass()`는 가상 함수가 아니며 객체의 `ClassPrivate`을 반환한다. 따라서 같은 C++ 구현을 사용하는 동적 클래스도 구분할 수 있다.

```cpp
Object->GetClass();       // 실제 런타임 클래스
AActor::StaticClass();    // AActor C++ 타입의 클래스
```

객체 생성 직후 `FUObjectManager`가 `ClassPrivate`을 한 번만 설정한다. 현재 생성자 실행 중에는 아직 클래스가 연결되지 않으므로 UObject 생성자와 파생 생성자에서 `GetClass()`를 호출하지 않는다.

### 객체 생성

`FUObjectManager::Create<T>()`는 `T::StaticClass()`를 실제 클래스로 사용한다. 동적 클래스는 명시적인 `UClass*`를 전달한다.

```cpp
T* Create();
T* Create(UClass* InClass);
```

`UClass`의 생성 함수는 생성 대상의 실제 클래스를 받는다.

```cpp
using FCreateObjectFunc = UObject* (*)(UClass* Class);
```

```text
DynamicClass::CreateObject
    ↓
Native CreateFunc(DynamicClass)
    ↓
new NativeType
    ↓
Object.ClassPrivate = DynamicClass
```

현재 기반은 동적 클래스 식별을 지원하지만 동적 클래스 등록 도구와 동적으로 추가되는 인스턴스 데이터 저장소는 아직 없다.

### 전역 객체 배열

모든 `UObject`는 생성될 때 UUID와 `GUObjectArray` 인덱스를 받는다. 파괴 시 마지막 원소를 제거 위치로 옮기는 swap-remove 방식을 사용하므로 `InternalIndex`는 영속 식별자가 아니다.

UUID와 인덱스 조회는 이미 생성되어 `GUObjectArray`에 등록된 객체만 반환한다. 둘 다 현재 실행 안에서만 유효하며 영속 식별자로 사용하지 않는다.

## 리플렉션 등록소와 코어 부트스트랩

`FReflectionRegistry`는 최상위 `UClass`, `UScriptStruct`, `UEnum`을 `unique_ptr`로 소유하고 `FName`으로 검색한다.

```text
FReflectionRegistry
├─ TArray<std::unique_ptr<UField>> Fields
└─ TMap<FName, UField*> FieldsByName
```

별도의 bootstrap 객체는 두지 않는다. `FReflectionRegistry::Startup()`이 활성 Registry를 설정한 뒤 `RegisterStaticClass()`를 호출한다.

```text
FName::Startup
    ↓
FReflectionRegistry::Startup
    ├─ GReflectionRegistry 연결
    └─ RegisterStaticClass
        ├─ 코어 UClass 저장 공간 생성
        ├─ 상속 관계 연결
        ├─ StaticClassPrivate 연결
        ├─ 각 UClass 객체의 ClassPrivate 연결
        └─ 부모 타입부터 Registry 등록
```

부트스트랩 대상은 다음과 같다.

```text
UObject
UField
UStruct
UClass
UScriptStruct
UFunction
UEnum
```

모든 코어 스키마 객체는 C++ 객체로서 `UClass`이므로 각 객체의 `ClassPrivate`은 `UClass::StaticClass()`를 가리킨다. `UClass` 스키마 자신은 자기 자신을 클래스로 가진다.

```cpp
UClass::StaticClass()->GetClass() == UClass::StaticClass();
```

종료 시 `ResetStaticClass()`가 정적 연결을 먼저 해제하고 Registry가 스키마를 파괴한다. 그 뒤 `FName`을 종료한다.

## 프로퍼티

`FProperty`는 컨테이너 안의 값 위치와 연산을 표현한다.

```text
Container pointer
    + Offset
    + ArrayIndex * ElementSize
    ↓
Property value
```

공통 연산은 초기화, 소멸, 복사, 직렬화와 참조 방문이다. `UStruct`는 직접 선언된 프로퍼티만 소유하고 상위 프로퍼티는 검색과 열거 시 순회한다.

`TNumericProperty<T>` 안에 int32, bool, float, double 프로퍼티를 함께 둔다. 다른 고정 폭 숫자 타입과 Map은 실제 요구가 생길 때 추가한다.

### 객체 프로퍼티

`FObjectProperty`는 raw `T*`와 `TObjectPtr<T>`를 모두 지원한다.

```text
FObjectProperty
├─ PropertyClass
└─ IObjectPtrOps
   ├─ TRawObjectPtrOps<T>
   └─ TObjectPtrOps<T>
```

`IObjectPtrOps`는 포인터 저장 형식의 초기화, 복사, 읽기, 쓰기와 참조 방문을 타입 소거한다. 강한 객체 참조의 현재 직렬화 형식은 런타임 UUID이며 같은 실행에서 이미 등록된 객체만 복원한다.

### 구조체와 배열

`UScriptStruct`는 `IStructOps`로 타입이 지워진 값의 수명을 관리한다. `TStructOps<T>`는 placement new, 명시적 소멸자 호출과 대입을 사용해 내부 동작을 직접 드러낸다.

`FStructProperty`는 내부 프로퍼티를 재귀 순회한다. `FArrayProperty`는 `FArrayOps`와 원소를 설명하는 `Inner` 프로퍼티를 조합한다. `TArray<bool>`은 `std::vector<bool>`의 proxy reference 때문에 현재 지원하지 않는다.

### 헤더 포함 정책

모든 프로퍼티를 모으는 `Properties.h`는 두지 않는다. 공통 연산만 필요한 코드는 `Object/Property.h`를 포함하고 등록 코드는 실제 생성하는 구체 프로퍼티 헤더만 포함한다.

## 함수 리플렉션

`UFunction`은 `UStruct`를 상속하며 매개변수와 반환값을 `FProperty`로 소유한다.

```text
UFunction Properties
├─ Parameter
├─ Parameter | OutParameter
└─ Parameter | ReturnParameter
```

현재 함수 플래그는 `Native`, `Callable`, `Const`만 제공한다. generated native invoker는 서로 다른 C++ 함수 형식을 공통 호출 형식으로 변환한다.

```cpp
using FNativeInvoker = void (*)(UObject* Context, void* Params);
```

`FScopedFunctionParams`는 정렬된 매개변수 메모리를 할당하고 `IStructOps`로 생성과 소멸을 짝지어 관리한다. 런타임 계약은 구현되어 있지만 generated invoker는 아직 없다.

## 객체 포인터와 참조 수집

`TObjectPtr<T>`는 하나의 `T*`를 저장하는 강한 참조 래퍼다. 참조 횟수, write barrier, relocation과 자동 무효화는 아직 제공하지 않는다.

`TSoftObjectPtr<T>`는 Week12 방식과 같이 물리 에셋 경로 `FString`과 선택적인 캐시 포인터만 직접 저장한다. 경로 변경 시 캐시를 비우며 자체적으로 객체를 검색하거나 에셋을 로드하지 않는다.

`ISoftObjectPtrOps`는 `FSoftObjectProperty`가 템플릿 인자와 무관하게 `TSoftObjectPtr`의 수명, 복사와 경로를 다루게 한다. `FSoftObjectProperty`는 물리 에셋 경로만 직렬화하고 강한 참조 수집에는 참여하지 않는다. 실제 경로 해석과 로딩은 향후 Asset Manager가 담당한다.

`FReferenceCollector`는 별도 handler 계층 없이 객체와 프로퍼티를 직접 순회한다.

```text
Root UObject
    ↓
FReferenceCollector
    ├─ UObject::AddReferencedObjects
    └─ Object->GetClass
        ↓
      UClass::GetAllProperties
        ↓
      FProperty::VisitReferencesInContainer
```

방문 집합이 순환 참조를 차단한다. 현재는 reachable 집합만 계산하며 root set 구성과 unreachable 객체 파괴는 아직 없다.

## 메타데이터와 직렬화

`FReflectionMetadata`는 `DisplayName`, `Category`, `Tooltip`을 보관한다. `UField`와 `FProperty`가 각각 메타데이터를 가지며 현재 모든 빌드에 포함된다.

프로퍼티 직렬화는 숫자 값, 문자열, 이름, enum 저장소, 구조체, 강한 객체 참조의 런타임 UUID, 소프트 객체 참조의 물리 에셋 경로와 배열 원소를 처리한다. 현재 형식은 프로퍼티 순서에 의존하며 schema version, field id, rename redirect와 지연 object fixup은 없다.

## 마커와 코드 생성

`ReflectionMacros.h`의 마커는 현재 C++ 동작을 만들지 않고 향후 parser가 읽을 선언 정보만 제공한다.

```cpp
UCLASS()
USTRUCT()
UENUM()
UPROPERTY()
UFUNCTION()
```

`GENERATED_CLASS`는 `StaticClass()` 진입점만 선언한다. `GetClass()`는 `UObject`가 객체의 `ClassPrivate`에서 공통으로 제공한다.

목표 생성 흐름은 다음과 같다.

```text
Marked header
    ↓ Reflection Header Tool
Generated code
    ├─ UClass / UScriptStruct / UEnum 생성과 등록
    ├─ 구체 FProperty 생성
    ├─ UFunction과 parameter property 생성
    ├─ native invoker 생성
    └─ StaticClassPrivate 연결
```

generated `.cpp`는 umbrella header 대신 필요한 프로퍼티 헤더만 직접 포함한다.

## 스레딩과 수명

현재 UObject 생성과 파괴, Registry 변경, 참조 수집과 프로퍼티 접근은 메인 스레드에서 수행한다.

- `GUObjectArray`와 Registry에는 동기화가 없다.
- 스키마 주소는 Registry 종료까지 유지한다.
- 생성 중인 UObject는 `ClassPrivate` 연결 전까지 `GetClass()`를 호출하지 않는다.
- 참조 수집 중에는 UObject와 스키마를 파괴하지 않는다.
- raw pointer와 `TObjectPtr`는 참조 대상을 직접 소유하지 않는다.

## 현재 구현 상태

### 구현됨

- UObject UUID와 `GUObjectArray` 등록 및 swap-remove
- 객체별 `ClassPrivate`과 타입별 `StaticClassPrivate` 분리
- 실제 `UClass*`를 전달하는 객체 생성 경로
- 코어 메타 타입의 two-phase bootstrap과 자기 참조 연결
- `UField`, `UStruct`, `UClass`, `UScriptStruct`, `UFunction`, `UEnum`
- 프로퍼티 소유, 상속 검색과 함수 스키마
- `IStructOps`, native invoker와 `FScopedFunctionParams`
- int32, bool, float, double, string, name, enum, struct, object, soft object, array property
- raw pointer와 `TObjectPtr`를 지원하는 `IObjectPtrOps`
- `FReflectionMetadata`의 표시 이름, 카테고리와 툴팁
- Registry 소유권, 이름 검색과 엔진 루프 수명 연결
- 중첩 강한 참조를 순회하는 `FReferenceCollector`

### 부분 구현

- native C++ 클래스의 generated 등록 코드는 아직 없다.
- 동적 `UClass`를 식별하고 생성 함수에 전달할 기반은 있지만 동적 클래스 생성 API는 없다.
- `TObjectPtr`에는 write barrier와 자동 무효화가 없다.
- `TSoftObjectPtr`는 물리 에셋 경로와 캐시만 보관하며 아직 경로를 해석하거나 에셋을 로드하지 않는다.
- 참조 수집기는 mark만 수행하고 sweep하지 않는다.
- 직렬화는 schema evolution을 지원하지 않는다.

### 미구현

- reflection parser와 generated `.h`/`.gen.cpp`
- 동적 클래스의 추가 인스턴스 데이터 저장소와 실행 모델
- CDO, object flags, root set과 mark/sweep GC
- weak object pointer와 generation 검증
- Asset Manager와 소프트 참조 비동기 로딩
- tagged/versioned serialization
- Map property
- 에디터 property customization
- hot reload, class reinstancing, replication과 Blueprint VM

## 관련 파일

- [Object.h](../KnotEngine/Source/Engine/Object/Object.h)
- [Object.cpp](../KnotEngine/Source/Engine/Object/Object.cpp)
- [Class.h](../KnotEngine/Source/Engine/Object/Class.h)
- [Class.cpp](../KnotEngine/Source/Engine/Object/Class.cpp)
- [Function.h](../KnotEngine/Source/Engine/Object/Function.h)
- [Property.h](../KnotEngine/Source/Engine/Object/Property.h)
- [ObjectPtr.h](../KnotEngine/Source/Engine/Object/ObjectPtr.h)
- [ReferenceCollector.h](../KnotEngine/Source/Engine/Object/ReferenceCollector.h)
- [ReflectionMacros.h](../KnotEngine/Source/Engine/Object/Reflection/ReflectionMacros.h)
- [ReflectionMetadata.h](../KnotEngine/Source/Engine/Object/Reflection/ReflectionMetadata.h)
- [ReflectionRegistry.h](../KnotEngine/Source/Engine/Object/Reflection/ReflectionRegistry.h)
- [ReflectionRegistry.cpp](../KnotEngine/Source/Engine/Object/Reflection/ReflectionRegistry.cpp)
- [Conventions.md](Conventions.md)
