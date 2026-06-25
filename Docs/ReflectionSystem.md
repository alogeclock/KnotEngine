# Reflection System Design

## 목표

KnotEngine의 리플렉션 시스템은 C++ 타입 정보를 런타임에 보존하여 에디터 노출, 저장, 복제, 직렬화, 스크립트 바인딩, RPC, GC 참조 추적, 향후 블루프린트 계층 확장을 하나의 메타데이터 체계로 처리하는 것을 목표로 한다.

핵심 원칙은 다음과 같다.

- `UClass`, `UScriptStruct`, `UFunction`, `UEnum`은 런타임 타입 메타데이터를 표현한다.
- `FProperty`는 멤버 변수, 함수 인자, 반환값의 메모리 위치와 타입별 동작을 표현한다.
- 에디터, 저장, 복제, Lua/Blueprint/RPC, GC는 모두 `FProperty` 순회를 공통 경로로 사용한다.
- C++ 마커는 사람이 쓰는 선언이며, 실제 등록 코드는 파서가 생성한 `.gen.cpp`가 담당한다.
- `FProperty`는 UE5처럼 타입별 subclass를 포인터로 저장한다.

## 리플렉션 처리 과정

1. C++ 헤더에 `UCLASS()`, `UPROPERTY()`, `USTRUCT()`, `UENUM()`, `UFUNCTION()`, `UMETA()` 등의 마커를 작성한다.
2. 리플렉션 파서가 헤더를 읽어 클래스, 구조체, enum, 함수, 프로퍼티 정보를 수집한다.
3. 파서는 타입별 `.gen.cpp` 파일을 생성한다.
4. generated code는 `UClass`, `UScriptStruct`, `UFunction`, `UEnum`, `FProperty`를 생성하고 메타데이터를 등록한다.
5. 모든 `UClass`는 전역 `FReflectionRegistry`에 등록된다.
6. 런타임 시스템은 `FReflectionRegistry`, `UClass`, `FProperty`를 통해 객체 생성, 에디터 노출, 저장, 복제, 직렬화, GC 참조 추적, 스크립트 바인딩, RPC 생성을 수행한다.

```text
Header Markers
  -> Reflection Parser
  -> *.gen.cpp
  -> StaticClass()/StaticStruct()
  -> UClass/UScriptStruct/UFunction/UEnum/FProperty registration
  -> FReflectionRegistry
  -> Editor / Serialize / Duplicate / GC / Script / RPC
```

## 매크로 역할

`UCLASS`, `UPROPERTY`, `USTRUCT`, `UENUM`, `UFUNCTION`, `UMETA`는 C++ 컴파일 시 직접 동작하지 않는 파서용 마커다.

`GENERATED_BODY`는 최소한 다음 진입점을 선언한다.

```cpp
#define GENERATED_BODY(ClassName, ParentClass) \
public: \
    using ThisClass = ClassName; \
    using Super = ParentClass; \
    static UClass* StaticClass(); \
    virtual UClass* GetClass() const override { return StaticClass(); } \
    friend struct Z_Construct_UClass_##ClassName;
```

구조체는 `GENERATED_STRUCT_BODY`를 통해 `StaticStruct()`를 선언한다.

```cpp
#define GENERATED_STRUCT_BODY(StructName) \
public: \
    static UScriptStruct* StaticStruct(); \
    friend struct Z_Construct_UScriptStruct_##StructName;
```

## 런타임 메타데이터 상속 구조

Unreal Engine의 구조를 참고하되, 자체 엔진의 구현 범위에 맞게 단순화한다.

```text
UObject
  UField
    UStruct
      UClass
      UScriptStruct
      UFunction
    UEnum
```

### UObject

모든 런타임 객체의 공통 기반이다.

- `UUID`
- `ObjectName`
- `InternalIndex`
- `GetClass()`
- `Serialize()`
- `Duplicate()`
- `PostDuplicate()`
- `PostEditProperty()`

모든 `UObject` 인스턴스는 자신이 어떤 런타임 클래스의 객체인지 `GetClass()`로 알려준다.

### UField

리플렉션 메타데이터의 공통 기반이다.

- `Name`
- `DisplayName`
- `Category`
- `MetaData`

`UField`는 클래스, 구조체, 함수, enum이 공유하는 이름과 표시 정보를 가진다.

### UStruct

메모리 레이아웃을 가진 메타데이터의 공통 기반이다. C++의 `struct` 키워드와 혼동하지 않는다.

- `SuperStruct`
- `StructureSize`
- `MinAlignment`
- `Properties`

`UClass`, `UScriptStruct`, `UFunction`은 모두 `UStruct`를 상속하며, 내부에 `FProperty` 목록을 가진다.

### UClass

C++ `UObject` 파생 클래스의 런타임 메타데이터다.

- `SuperClass`
- `ClassFlags`
- `CreateFunc`
- `Functions`
- `Properties`

`UClass`는 `UObject` 시스템 내부에 존재하는 메타 객체다. 모든 `UClass`는 `FReflectionRegistry`에 등록되며, 메타 객체도 엔진 루트로 보호되어야 한다. 구현 정책에 따라 `GUObjectArray`에 포함할 수 있으며, 포함한다면 GC에서 항상 root로 취급한다.

UE의 `UClass`는 CDO(Class Default Object)를 가진다. KnotEngine 1차 구현에서는 CDO를 구현하지 않는다. 대신 default constructor와 generated property initialization을 이용한다. 향후 Blueprint, prefab, archetype, default diff가 필요해지면 CDO를 추가한다.

### UScriptStruct

C++ 값 타입 구조체의 런타임 메타데이터다.

- `StructOps`
- `Properties`

`UScriptStruct`는 구조체 생성, 파괴, 기본 복사를 위한 `IStructOps`를 가진다. 단, 리플렉션 기반 복제, 직렬화, GC 방문은 단순 `StructOps->Copy()`만 사용하지 않고 내부 `FProperty`를 재귀 순회한다.

### UFunction

C++ 멤버 함수의 런타임 메타데이터다.

- `OuterClass`
- `FunctionFlags`
- `ParamsSize`
- `NativeFuncPtr`
- `Properties`

`UFunction`의 `Properties`에는 함수 인자와 반환값이 들어간다. 인자는 `Parm`, 반환값은 `ReturnParm` 플래그를 가진다.

### UEnum

enum 이름과 값, 표시 이름을 보관한다.

```cpp
struct FEnumValue
{
    FName Name;
    FName DisplayName;
    int64 Value;
};
```

`UEnum`은 enum 값과 표시 이름을 단순 매핑하며, 에디터 콤보박스, 직렬화, Lua/Blueprint 변환에 사용한다.

## FReflectionRegistry

`FReflectionRegistry`는 전역 싱글턴 리플렉션 등록소다.

```cpp
class FReflectionRegistry
{
public:
    void RegisterClass(UClass* Class);
    void RegisterStruct(UScriptStruct* Struct);
    void RegisterEnum(UEnum* Enum);

    UClass* FindClass(FName ClassName) const;
    UScriptStruct* FindStruct(FName StructName) const;
    UEnum* FindEnum(FName EnumName) const;

    void GetClassesDerivedFrom(const UClass* BaseClass, TArray<UClass*>& OutClasses) const;
};
```

등록은 명시적 초기화 단계에서 수행한다.

```cpp
void InitializeReflection()
{
    RegisterGeneratedReflection();
}
```

static auto-registration은 편리하지만 C++ static initialization order 문제를 만들 수 있다. 따라서 generated code는 등록 함수를 제공하고, 엔진 초기화 순서에서 명시적으로 호출하는 방식을 기본 정책으로 한다.

## FProperty 설계

KnotEngine은 `FProperty` 단일 descriptor 방식이 아니라 타입별 subclass 방식을 사용한다.

```cpp
class FProperty
{
public:
    virtual ~FProperty() = default;

    FName GetName() const;
    FName GetDisplayName() const;
    size_t GetOffset() const;
    size_t GetSize() const;
    EPropertyFlags GetFlags() const;

    void* ContainerPtrToValuePtr(void* Container) const;
    const void* ContainerPtrToValuePtr(const void* Container) const;

    virtual void SerializeValue(FArchive& Ar, void* ValuePtr) const = 0;
    virtual void CopyValue(void* DstValuePtr, const void* SrcValuePtr, FDuplicateContext& Context) const = 0;
    virtual void VisitReferences(void* ValuePtr, FReferenceCollector& Collector) const {}
    virtual void VisitSoftReferences(void* ValuePtr, FSoftReferenceCollector& Collector) const {}

private:
    FName Name;
    FName DisplayName;
    FName Category;
    size_t Offset = 0;
    size_t Size = 0;
    EPropertyFlags Flags = EPropertyFlags::None;
    FMetaData MetaData;
};
```

타입별 property는 필요한 메타데이터만 가진다.

```cpp
class FIntProperty : public FProperty {};
class FFloatProperty : public FProperty {};
class FBoolProperty : public FProperty {};

class FEnumProperty : public FProperty
{
    UEnum* Enum = nullptr;
};

class FObjectProperty : public FProperty
{
    UClass* PropertyClass = nullptr;
    EObjectReferenceKind ReferenceKind = EObjectReferenceKind::RuntimeObject;
    const IObjectPtrOps* ObjectPtrOps = nullptr;
};

class FSoftObjectProperty : public FProperty
{
    UClass* ExpectedClass = nullptr;
    const ISoftObjectPtrOps* SoftObjectPtrOps = nullptr;
};

class FStructProperty : public FProperty
{
    UScriptStruct* Struct = nullptr;
};

class FArrayProperty : public FProperty
{
    FProperty* InnerProperty = nullptr;
    const IArrayPropertyOps* ArrayOps = nullptr;
};
```

### FProperty 소유권

`FProperty` 메타데이터는 UObject 참조가 아니므로 `TObjectPtr`로 보관하지 않는다. `UStruct`가 소유하고, 조회용으로 raw pointer를 제공한다.

```cpp
class UStruct : public UField
{
private:
    TArray<TUniquePtr<FProperty>> OwnedProperties;
    TArray<FProperty*> Properties;
};
```

등록이 끝난 `UStruct`는 `Seal()`되어 더 이상 property 추가를 허용하지 않는다. 이렇게 하면 `FProperty*` 포인터 안정성을 보장할 수 있다.

```cpp
void UStruct::Seal()
{
    bSealed = true;
}
```

### FPropertyParams

generated code는 property subclass 생성에 필요한 초기화 구조체를 사용한다.

```cpp
struct FPropertyParams
{
    FName Name;
    FName DisplayName;
    FName Category;
    size_t Offset;
    size_t Size;
    EPropertyFlags Flags;
    FMetaData MetaData;
};

struct FObjectPropertyParams : FPropertyParams
{
    UClass* PropertyClass = nullptr;
    EObjectReferenceKind ReferenceKind = EObjectReferenceKind::RuntimeObject;
    const IObjectPtrOps* ObjectPtrOps = nullptr;
};

struct FArrayPropertyParams : FPropertyParams
{
    FProperty* InnerProperty = nullptr;
    const IArrayPropertyOps* ArrayOps = nullptr;
};
```

`FPropertyParams`는 codegen과 런타임 메타 객체 생성의 경계다. 파서는 C++ 타입과 metadata를 분석한 뒤 적절한 params를 생성한다.

### FPropertyHandle

에디터와 스크립트 시스템은 객체와 프로퍼티를 묶은 handle로 값을 다룬다.

```cpp
struct FPropertyHandle
{
    UObject* Owner = nullptr;
    FProperty* Property = nullptr;

    bool IsValid() const;
    void* GetValuePtr() const;
    bool SetValueFromEditor(const void* Value);
};
```

`FPropertyHandle`은 직접 메모리 offset을 계산하지 않고 `FProperty`의 `ContainerPtrToValuePtr()`을 사용한다.

## 타입별 처리 원칙

### Primitive

`int`, `bool`, `float` 등은 `ValuePtr`을 타입 캐스팅하여 읽고 쓴다.

```cpp
void FFloatProperty::CopyValue(void* Dst, const void* Src, FDuplicateContext&) const
{
    *static_cast<float*>(Dst) = *static_cast<const float*>(Src);
}
```

### Enum

`FEnumProperty`는 `UEnum*`을 보관한다.

- 에디터: `FEnumValue` 목록으로 콤보박스 구성
- 저장: 정수값 또는 이름 저장
- Lua/Blueprint: 이름 문자열 또는 정수값 변환

### Struct

`FStructProperty`는 `UScriptStruct*`를 보관한다.

복제와 직렬화는 반드시 구조체 내부 property를 재귀 순회한다.

```text
FStructProperty::CopyValue
  -> Struct->GetProperties()
  -> ChildProperty->CopyValue(Dst + Offset, Src + Offset, Context)
```

이 규칙은 `struct` 안에 `TObjectPtr`, `TArray<TObjectPtr<T>>`, 다른 `USTRUCT`가 들어가도 DuplicateContext와 GC 참조 추적이 정상 동작하게 한다.

`IStructOps`는 생성, 파괴, fallback copy에 사용한다.

```cpp
struct IStructOps
{
    virtual void Construct(void* Ptr) const = 0;
    virtual void Destruct(void* Ptr) const = 0;
    virtual void Copy(void* Dst, const void* Src) const = 0;
};
```

### Array

`FArrayProperty`는 `InnerProperty`와 `IArrayPropertyOps`를 가진다.

```cpp
struct IArrayPropertyOps
{
    virtual int32 Num(const void* ArrayPtr) const = 0;
    virtual void Resize(void* ArrayPtr, int32 NewNum) const = 0;
    virtual void* GetElementPtr(void* ArrayPtr, int32 Index) const = 0;
};
```

복제, 직렬화, GC 방문은 모두 `InnerProperty`로 재귀 처리한다.

```text
FArrayProperty::CopyValue
  -> Resize
  -> for each element
       InnerProperty->CopyValue(DstElement, SrcElement, Context)
```

### Object Pointer

`TObjectPtr<T>`와 raw `T*`는 `FObjectProperty`가 처리한다.

```cpp
struct IObjectPtrOps
{
    virtual UObject* Get(const void* ValuePtr) const = 0;
    virtual void Set(void* ValuePtr, UObject* Object) const = 0;
    virtual void VisitReference(void* ValuePtr, FReferenceCollector& Collector) const = 0;
};
```

`TObjectPtr<T>`는 hard reference다.

- GC: `VisitReference()` 대상
- Duplicate: `FDuplicateContext`에 복제본이 있으면 복제본으로 remap
- Serialize: runtime object reference는 UUID/object id로 저장

```text
Source.Target = OriginalComponent
Context[OriginalComponent] = DuplicatedComponent
Duplicate result Target = DuplicatedComponent
```

### Soft Object Pointer

`TSoftObjectPtr<T>`는 hard reference가 아니다.

- GC: object를 mark하지 않음
- Serialize: asset path 저장
- Load: 필요 시 asset path로 resolve

```cpp
struct ISoftObjectPtrOps
{
    virtual FString GetPath(const void* ValuePtr) const = 0;
    virtual void SetPath(void* ValuePtr, const FString& Path) const = 0;
};
```

## 에디터 노출

에디터는 `FReflectionRegistry`와 `FProperty`를 통해 자동 UI를 구성한다.

### Details Panel

1. 선택된 `UObject`의 `GetClass()`를 얻는다.
2. `UClass::GetAllProperties()`로 상속 체인의 property를 모두 수집한다.
3. `Edit` 플래그가 있는 property만 표시한다.
4. property subclass에 따라 적절한 위젯을 선택한다.
5. 변경 시 `FPropertyHandle`로 값을 쓰고 `PostEditProperty(PropertyName)`를 호출한다.

### Place Actor

`Place Actor` 위젯은 모든 `AActor` 파생 클래스를 찾는다.

```cpp
TArray<UClass*> Classes;
FReflectionRegistry::Get().GetClassesDerivedFrom(AActor::StaticClass(), Classes);
```

노출 조건은 다음과 같다.

- `AActor` 파생 클래스
- `CF_Placeable`
- `!CF_Abstract`

`Placeable`, `DisplayName`, `Category` 등은 `UCLASS(...)` metadata에서 온다.

### Add Component

`Add Component` 위젯은 모든 `UActorComponent` 파생 클래스를 찾는다.

```cpp
TArray<UClass*> Classes;
FReflectionRegistry::Get().GetClassesDerivedFrom(UActorComponent::StaticClass(), Classes);
```

노출 조건은 다음과 같다.

- `UActorComponent` 파생 클래스
- `CF_SpawnableComponent`
- `!CF_Abstract`

## Duplicate

복제는 `FProperty` 기반 메모리 복사와 참조 remap을 조합한다.

```text
UObject::Duplicate(Context)
  -> NewObject(GetClass())
  -> DuplicatedObject->CopyPropertiesFrom(this, Context)
  -> DuplicatedObject->PostDuplicate(this)
```

### NewObject

```cpp
UObject* NewObject(UClass* Class)
{
    if (!Class || Class->HasAnyClassFlags(CF_Abstract))
    {
        return nullptr;
    }
    return Class->CreateObject();
}
```

`UClass::CreateObject()`는 generated code가 등록한 `CreateFunc`를 호출한다.

```cpp
using FCreateObjectFunc = UObject* (*)();
```

### CopyPropertiesFrom

```text
CopyPropertiesFrom
  -> Src->GetClass()->GetAllProperties()
  -> Dst->GetClass()->FindProperty(Name)
  -> DstProperty->CopyValue(DstValuePtr, SrcValuePtr, Context)
```

`Transient` property는 복제하지 않는다.

### FDuplicateContext

`FDuplicateContext`는 원본 객체와 복제 객체의 매핑을 가진다.

```cpp
struct FDuplicateContext
{
    void Add(UObject* Original, UObject* Duplicate);
    UObject* Resolve(UObject* Original) const;
};
```

`FObjectProperty`는 복제 중 참조가 이미 복제된 객체를 가리키면 복제본으로 교체한다. 매핑이 없으면 원본 포인터를 유지하거나 정책에 따라 null 처리한다.

### PostDuplicate

`PostDuplicate(Original)`은 단순 property 복사로 해결되지 않는 후처리를 담당한다.

- owner/parent/child 관계 복구
- component attachment 복구
- runtime resource 재생성
- asset reload
- event binding 재연결

## Serialize

직렬화는 archive 종류에 따라 JSON, binary, memory snapshot으로 확장할 수 있다.

```text
UObject::Serialize(Ar)
  -> Type 저장
  -> ObjectName 저장
  -> SerializeProperties(Ar)
```

`SerializeProperties()`는 클래스의 모든 property를 순회한다.

```text
SerializeProperties
  -> GetClass()->GetAllProperties()
  -> Property->SerializeItem(Ar, Object)
```

`FProperty::SerializeItem()`은 container 주소와 offset으로 값 주소를 찾은 뒤 `SerializeValue()`를 호출한다.

### Object Graph Serialize

일반 object pointer는 저장 파일에서 안정적인 object id로 저장한다.

```text
Save
  -> graph traversal
  -> object id 할당
  -> ObjectPtr property는 object id 저장

Load
  -> 모든 object를 먼저 생성
  -> id -> object resolver 구성
  -> property load 단계에서 object id를 pointer로 resolve
```

### Asset Serialize

asset reference는 object id가 아니라 asset path로 저장한다.

```text
TObjectPtr<UStaticMesh> with ReferenceKind=Asset
  -> path serialize

TSoftObjectPtr<UStaticMesh>
  -> path serialize
```

hard asset pointer와 soft asset pointer는 모두 path 저장이 가능하지만, GC 정책은 다르다.

## GC

GC는 mark-sweep 기반으로 시작한다.

### Object Storage

모든 런타임 `UObject`는 `GUObjectArray`에 등록된다.

```cpp
TArray<UObject*> GUObjectArray;
```

메타 객체(`UClass`, `UScriptStruct`, `UEnum`, `UFunction`)를 `GUObjectArray`에 포함한다면 엔진 root로 항상 mark해야 한다. 포함하지 않는다면 `FReflectionRegistry`가 별도 lifetime을 보장한다.

### Root Set

root 후보는 다음과 같다.

- `UEngine`
- active `UWorld`
- loaded asset root
- editor selection
- reflection metadata roots
- explicit `AddToRoot()` 객체

### Mark

```text
for Root in RootSet
  MarkObject(Root)

MarkObject(Object)
  -> if already marked return
  -> mark
  -> Object->GetClass()->GetAllProperties()
  -> Property->VisitReferences(ValuePtr, Collector)
  -> recursively MarkObject(ReferencedObject)
```

`FObjectProperty`, `FArrayProperty`, `FStructProperty`는 반드시 재귀 방문을 지원한다.

### Sweep

```text
for Object in GUObjectArray
  if !Marked && !Rooted
    DestroyObject(Object)
  else
    ClearMarkedFlag(Object)
```

`TSoftObjectPtr`는 mark 대상이 아니다. `TWeakObjectPtr`를 도입하면 weak pointer table 또는 object serial number를 통해 sweep 이후 자동 invalidation을 처리한다.

## Lua, Blueprint, RPC 확장

### Lua

Lua 바인딩은 `UClass`, `UFunction`, `FProperty`를 사용한다.

- `LuaReadOnly`: getter만 노출
- `LuaReadWrite`: getter/setter 노출
- `LuaCallable`: 함수 호출 노출

함수 호출은 `UFunction`의 params struct를 생성한 뒤 property 변환을 통해 값을 채운다.

```text
Lua call
  -> FindFunction(Name)
  -> Allocate params memory
  -> Param FProperty로 Lua 값을 C++ 값으로 변환
  -> UObject::ProcessEvent(Function, Params)
  -> ReturnParm을 Lua 값으로 변환
```

### Blueprint

블루프린트는 `UClass`와 `FProperty`를 런타임에 확장하는 계층으로 설계한다.

Epic 문서 기준으로 UE의 `UBlueprintGeneratedClass`는 `UClass`를 상속하며 Blueprint에서 생성된 클래스 메타데이터를 표현한다. 또한 component template, dynamic binding, property guid, replication list, uber graph function 같은 블루프린트 전용 데이터를 가진다.

KnotEngine의 1차 목표는 다음과 같다.

```cpp
class UBlueprintGeneratedClass : public UClass
{
public:
    UClass* NativeParentClass = nullptr;
    TArray<FProperty*> BlueprintProperties;
    TArray<UFunction*> BlueprintFunctions;
    TArray<UActorComponent*> ComponentTemplates;
};
```

예를 들어 Blueprint에서 `ACharacter`에 `Mp`, `Speed`를 추가하면 다음처럼 처리한다.

```text
ACharacter::StaticClass()
  -> native UClass

BP_ACharacter
  -> UBlueprintGeneratedClass
  -> SuperClass = ACharacter::StaticClass()
  -> Adds FFloatProperty Mp
  -> Adds FFloatProperty Speed
```

에디터 노출, 복제, 직렬화 시에는 항상 super property를 먼저 순회하고, blueprint class가 추가한 property를 뒤이어 순회한다.

```text
GetAllProperties(BP_ACharacter)
  -> ACharacter properties
  -> BP_ACharacter properties
```

### Blueprint Object Memory

native C++ 객체는 `new T()`로 생성할 수 있지만, blueprint class는 native class보다 더 큰 instance data가 필요할 수 있다.

1차 구현은 다음 중 하나를 선택한다.

#### Option A: Extension Storage

native object는 그대로 생성하고, blueprint 추가 property는 별도 byte buffer에 저장한다.

```cpp
class UObject
{
    TMap<UClass*, TArray<uint8>> DynamicPropertyStorage;
};
```

장점은 구현이 쉽고 native layout을 건드리지 않는다는 것이다. 단점은 native property와 dynamic property의 주소 계산 경로가 달라진다.

#### Option B: Custom Allocation

`UClass::CreateObject()`가 native size + blueprint property size만큼 메모리를 할당하고 object를 placement-new로 생성한다.

장점은 하나의 memory block에서 property offset을 통일할 수 있다는 것이다. 단점은 생성, 파괴, alignment, constructor 호출 관리가 복잡하다.

KnotEngine 1차 구현은 Option A를 우선한다. 이후 Blueprint VM과 CDO가 들어오면 Option B 또는 별도 UObject instance layout으로 확장한다.

### RPC

RPC는 `UFunction` metadata 위에 구축한다.

- `Server`
- `Client`
- `NetMulticast`
- `Reliable`
- `Unreliable`

파서는 `UFUNCTION(Server, Reliable)` 같은 metadata를 `EFunctionFlags`로 변환한다.

```text
Call RPC function
  -> UFunction metadata 확인
  -> Params property를 binary archive에 serialize
  -> socket packet 생성
  -> receiver에서 UFunction lookup
  -> params deserialize
  -> ProcessEvent
```

## Obj->GetClass()와 AActor::StaticClass()

```cpp
Obj->GetClass()
```

객체의 실제 런타임 클래스를 반환한다. Blueprint class 인스턴스라면 `BP_ACharacter`의 `UBlueprintGeneratedClass`를 반환할 수 있다.

```cpp
AActor::StaticClass()
```

C++ 타입 `AActor` 자체의 `UClass` 싱글턴을 반환한다.

예시:

```cpp
AActor* Obj = NewObject(BP_ACharacterClass);

Obj->GetClass();        // BP_ACharacterClass
AActor::StaticClass();  // native AActor class
Obj->IsA(AActor::StaticClass()); // true
```

## Generated Code 예시

```cpp
struct Z_Construct_UClass_USceneComponent
{
    static void RegisterProperties(UClass* Class)
    {
        FStructPropertyParams Params;
        Params.Name = "RelativeLocation";
        Params.Offset = offsetof(USceneComponent, RelativeLocation);
        Params.Size = sizeof(USceneComponent::RelativeLocation);
        Params.Flags = EPropertyFlags::Read | EPropertyFlags::Write | EPropertyFlags::Edit;
        Params.Struct = FVector::StaticStruct();

        Class->AddProperty(MakeUnique<FStructProperty>(Params));
    }
};

UClass* USceneComponent::StaticClass()
{
    static UClass* Class = nullptr;
    if (!Class)
    {
        Class = new UClass(
            "USceneComponent",
            UActorComponent::StaticClass(),
            sizeof(USceneComponent),
            CF_Component | CF_SpawnableComponent,
            []() -> UObject* { return new USceneComponent(); });

        Z_Construct_UClass_USceneComponent::RegisterProperties(Class);
        Class->Seal();
        FReflectionRegistry::Get().RegisterClass(Class);
    }
    return Class;
}
```

실제 구현에서는 static initialization order를 피하기 위해 `RegisterGeneratedReflection()`에서 `StaticClass()` 호출 순서를 제어한다.

## 구현 단계

### Phase 1: Runtime Core

- `UObject`
- `UField`
- `UStruct`
- `UClass`
- `UScriptStruct`
- `UFunction`
- `UEnum`
- `FReflectionRegistry`
- `StaticClass()`, `StaticStruct()`

### Phase 2: FProperty Subclass

- `FProperty`
- `FNumericProperty`
- `FBoolProperty`
- `FEnumProperty`
- `FObjectProperty`
- `FSoftObjectProperty`
- `FStructProperty`
- `FArrayProperty`
- `FPropertyHandle`
- `FPropertyParams`

### Phase 3: Codegen

- header scanner
- balanced macro parser
- metadata parser
- `.gen.cpp` generation
- generated registration entrypoint
- clear error message with file and line

### Phase 4: Editor/Serialize/Duplicate

- Details panel
- Place Actor
- Add Component
- `UObject::Serialize()`
- object graph serializer
- `UObject::Duplicate()`
- `FDuplicateContext`

### Phase 5: GC

- `GUObjectArray`
- root set
- mark-sweep
- hard reference traversal
- soft reference collection
- weak pointer invalidation

### Phase 6: Script, RPC, Blueprint

- Lua property binding
- Lua function binding
- RPC serialization over `UFunction`
- `UBlueprintGeneratedClass`
- dynamic property storage
- component templates
- future CDO/archetype support

## 설계상 주의점

- `FProperty*`는 metadata pointer이며 GC hard pointer가 아니다.
- 실제 UObject 참조만 `TObjectPtr<T>` hard reference로 취급한다.
- `FStructProperty` 복제는 `StructOps->Copy()`만 사용하면 안 된다. 반드시 내부 property를 재귀 복제한다.
- `FArrayProperty`는 element가 object pointer 또는 struct일 수 있으므로 항상 `InnerProperty`로 재귀 처리한다.
- 등록 완료 후 `UStruct`는 sealed 상태가 되어야 한다.
- generated code는 static auto-registration보다 명시적 initialization을 우선한다.
- `Transient`는 저장하지 않는다. 복제 여부는 별도 flag로 분리할 수 있다.
- `SaveGame`, `Edit`, `LuaReadWrite`, `Replicated`, `BlueprintReadWrite`는 서로 다른 정책이므로 장기적으로 flag namespace를 분리한다.
- Blueprint dynamic property는 native C++ object layout과 다른 문제이므로 별도 storage 정책을 문서화하고 시작한다.

## 참고

- Epic Games, UBlueprintGeneratedClass API: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UBlueprintGeneratedClass
