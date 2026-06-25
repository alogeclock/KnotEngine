# Serialization Architecture Design

## 목표

KnotEngine의 직렬화 시스템은 `FArchive` 하나에 모든 책임을 몰아넣지 않고, 리플렉션 메타데이터와 타입별 serializer를 중심으로 설계한다.

핵심 목표는 다음과 같다.

- 직렬화, 역직렬화, 복제, GC 참조 방문, 스크립트 변환이 같은 타입 메타데이터를 사용한다.
- `FArchive`는 raw binary stream 또는 낮은 수준의 byte IO 역할로 제한한다.
- JSON, tagged binary, save game, asset package 같은 포맷은 별도 backend로 분리한다.
- `FProperty`는 자기 타입을 처리할 serializer 또는 ops를 가진다.
- struct, array, object pointer는 항상 재귀적으로 처리한다.
- object reference는 저장 포맷이 아니라 serialization context의 resolver 정책으로 처리한다.

```text
Reflection Metadata
  -> Property Serializer
  -> Serialization Context
  -> Value Reader / Value Writer
  -> Format Backend
```

## 문제의식

Unreal Engine의 `FArchive`는 오래된 중심축이며, byte stream 기반 저장과 로드에는 매우 잘 맞는다.

```cpp
Ar << Health;
Ar << Speed;
```

하지만 엔진이 커지면 다음 요구가 생긴다.

- property 이름과 함께 저장해야 한다.
- 없는 필드는 건너뛰어야 한다.
- 버전이 달라도 읽을 수 있어야 한다.
- JSON처럼 사람이 읽을 수 있는 포맷이 필요하다.
- object, array, map 같은 계층 구조가 필요하다.
- object pointer를 파일 내부 object id나 asset path로 바꿔 저장해야 한다.
- save game, editor asset, network packet, duplicate가 서로 다른 정책을 가져야 한다.

이 요구를 `FArchive` 하나에 계속 추가하면 `CurrentKey`, `BeginObject`, `BeginArray`, `SerializeBytes`, `ObjectResolver`, `HasKey`, version, error state가 한 클래스에 섞인다. 결과적으로 archive가 포맷, 타입, object policy를 모두 아는 거대한 관문 객체가 된다.

따라서 KnotEngine은 archive 중심 설계가 아니라 property serializer 중심 설계를 기본 방향으로 삼는다.

## 핵심 구조

```text
FProperty
  metadata + serializer pointer

IPropertySerializer
  type-specific save/load/copy/reference visit/script conversion

FSerializationContext
  목적, 버전, object resolver, property filter, error reporter

IValueWriter / IValueReader
  포맷 중립적인 structured value 입출력

Format Backend
  JSON, tagged binary, memory tree, package writer

IByteStream 또는 FArchive
  낮은 수준의 binary byte IO
```

## FProperty와 Serializer

`FProperty`는 멤버 변수의 이름, offset, 크기, flags를 가진다. 타입별 동작은 `IPropertySerializer`에 위임한다.

```cpp
struct FProperty
{
    FName Name;
    FName DisplayName;
    size_t Offset = 0;
    size_t Size = 0;
    EPropertyFlags Flags = EPropertyFlags::None;

    const IPropertySerializer* Serializer = nullptr;
};
```

```cpp
struct IPropertySerializer
{
    virtual ~IPropertySerializer() = default;

    virtual void Save(
        const FProperty& Property,
        const void* Value,
        FSerializationContext& Context,
        IValueWriter& Writer) const = 0;

    virtual void Load(
        const FProperty& Property,
        void* Value,
        FSerializationContext& Context,
        IValueReader& Reader) const = 0;

    virtual void Copy(
        const FProperty& Property,
        void* Dst,
        const void* Src,
        FDuplicateContext& Context) const = 0;

    virtual void VisitReferences(
        const FProperty& Property,
        void* Value,
        FReferenceCollector& Collector) const {}
};
```

이 방식의 장점은 중앙 `switch(Type)`가 커지지 않는다는 점이다.

```text
FIntSerializer
FFloatSerializer
FBoolSerializer
FEnumSerializer
FStructSerializer
FArraySerializer
FObjectPtrSerializer
FSoftObjectPtrSerializer
```

타입이 늘어나면 serializer만 추가하면 된다.

## Value Reader / Writer

`IValueWriter`와 `IValueReader`는 byte stream이 아니라 포맷 중립적인 값 입출력 계층이다.

```cpp
struct IValueWriter
{
    virtual ~IValueWriter() = default;

    virtual void BeginObject(FName Name) = 0;
    virtual void EndObject() = 0;

    virtual void BeginArray(FName Name, int32 Count) = 0;
    virtual void EndArray() = 0;

    virtual void WriteBool(FName Name, bool Value) = 0;
    virtual void WriteInt(FName Name, int32 Value) = 0;
    virtual void WriteFloat(FName Name, float Value) = 0;
    virtual void WriteString(FName Name, FStringView Value) = 0;
    virtual void WriteName(FName Name, FName Value) = 0;
    virtual void WriteObjectRef(FName Name, FObjectId Value) = 0;
};
```

```cpp
struct IValueReader
{
    virtual ~IValueReader() = default;

    virtual bool BeginObject(FName Name) = 0;
    virtual void EndObject() = 0;

    virtual bool BeginArray(FName Name, int32& OutCount) = 0;
    virtual void EndArray() = 0;

    virtual bool ReadBool(FName Name, bool& OutValue) = 0;
    virtual bool ReadInt(FName Name, int32& OutValue) = 0;
    virtual bool ReadFloat(FName Name, float& OutValue) = 0;
    virtual bool ReadString(FName Name, FString& OutValue) = 0;
    virtual bool ReadName(FName Name, FName& OutValue) = 0;
    virtual bool ReadObjectRef(FName Name, FObjectId& OutValue) = 0;
};
```

JSON backend는 이를 JSON object와 array로 저장한다.

```text
WriteInt("Health", 100)
  -> { "Health": 100 }
```

Tagged binary backend는 이름 hash, type tag, payload로 저장할 수 있다.

```text
WriteInt("Health", 100)
  -> [NameId][Type:Int][Payload:4 bytes]
```

Property serializer는 JSON인지 binary인지 알 필요가 없다.

## Serialization Context

`FSerializationContext`는 저장 포맷이 아니라 직렬화 목적과 정책을 담는다.

```cpp
enum class ESerializationPurpose
{
    Asset,
    SaveGame,
    Duplicate,
    Network,
    EditorCopyPaste,
};
```

```cpp
struct FSerializationContext
{
    ESerializationPurpose Purpose = ESerializationPurpose::Asset;

    FObjectReferenceResolver* ObjectResolver = nullptr;
    FVersionContainer Versions;
    EPropertyFilterFlags PropertyFilter = EPropertyFilterFlags::None;
    FErrorReporter* ErrorReporter = nullptr;

    bool bIncludeEditorOnly = true;
    bool bAllowMissingFields = true;
};
```

예를 들어 같은 `TObjectPtr`이라도 목적에 따라 저장 정책이 달라질 수 있다.

```text
Asset
  runtime object -> object graph id
  asset object   -> asset path

SaveGame
  save game object -> save object id
  transient object -> skip or null

Network
  replicated object -> net id
```

이 정책은 writer가 아니라 context와 object resolver가 담당한다.

## 타입별 Serializer 예시

### Primitive

```cpp
struct FIntSerializer final : IPropertySerializer
{
    void Save(
        const FProperty& Property,
        const void* Value,
        FSerializationContext&,
        IValueWriter& Writer) const override
    {
        Writer.WriteInt(Property.Name, *static_cast<const int32*>(Value));
    }

    void Load(
        const FProperty& Property,
        void* Value,
        FSerializationContext&,
        IValueReader& Reader) const override
    {
        int32 LoadedValue = 0;
        if (Reader.ReadInt(Property.Name, LoadedValue))
        {
            *static_cast<int32*>(Value) = LoadedValue;
        }
    }
};
```

### Struct

`FStructSerializer`는 `UScriptStruct`의 child property를 재귀 순회한다.

```text
Save struct
  -> BeginObject(Property.Name)
  -> for ChildProperty in ScriptStruct.Properties
       ChildProperty.Serializer->Save(...)
  -> EndObject
```

복제도 동일하게 재귀 처리한다.

```text
Copy struct
  -> for ChildProperty in ScriptStruct.Properties
       ChildProperty.Serializer->Copy(...)
```

이 원칙은 다음 구조를 안전하게 처리하기 위해 필요하다.

```cpp
USTRUCT()
struct FMyStruct
{
    UPROPERTY()
    TObjectPtr<UObject> Target;
};
```

struct를 통째로 대입하면 `Target`이 duplicate context로 remap되지 않는다. 따라서 struct 내부 property도 반드시 serializer를 통해 복제해야 한다.

### Array

`FArraySerializer`는 `InnerProperty`와 `IArrayPropertyOps`를 사용한다.

```text
Save array
  -> BeginArray(Property.Name, Count)
  -> for each element
       InnerProperty.Serializer->Save(...)
  -> EndArray
```

```text
Load array
  -> BeginArray(Property.Name, Count)
  -> ArrayOps.Resize(Count)
  -> for each element
       InnerProperty.Serializer->Load(...)
  -> EndArray
```

array element가 struct 또는 object pointer여도 동일한 경로로 처리된다.

### Object Pointer

`FObjectPtrSerializer`는 포인터 값을 직접 저장하지 않는다.

```text
Save TObjectPtr
  -> UObject* Object = ObjectPtrOps.Get(Value)
  -> FObjectId Id = Context.ObjectResolver->GetObjectId(Object)
  -> Writer.WriteObjectRef(Property.Name, Id)
```

```text
Load TObjectPtr
  -> Reader.ReadObjectRef(Property.Name, Id)
  -> UObject* Object = Context.ObjectResolver->ResolveObjectId(Id, ExpectedClass)
  -> ObjectPtrOps.Set(Value, Object)
```

GC 방문도 serializer가 처리한다.

```text
VisitReferences
  -> ObjectPtrOps.Get(Value)
  -> Collector.AddReferencedObject(Object)
```

### Soft Object Pointer

`TSoftObjectPtr<T>`는 hard reference가 아니므로 GC mark 대상이 아니다.

```text
Save SoftObjectPtr
  -> asset path 저장

Load SoftObjectPtr
  -> asset path 복원

VisitReferences
  -> 아무것도 하지 않음
```

## FArchive의 위치

이 설계에서 `FArchive`는 전체 직렬화 시스템의 중심이 아니다.

권장 역할은 다음 중 하나다.

```text
Option A
  FArchive = raw binary byte stream

Option B
  IByteStream = raw byte stream
  FArchive 이름은 사용하지 않음
```

최소 binary stream 인터페이스는 다음 정도로 제한한다.

```cpp
struct IByteStream
{
    virtual ~IByteStream() = default;
    virtual bool IsReading() const = 0;
    virtual bool IsWriting() const = 0;
    virtual void SerializeBytes(void* Data, uint64 Size) = 0;
};
```

`FBinaryValueWriter`는 내부적으로 `IByteStream`을 사용해 tagged binary를 쓸 수 있다.

```text
FPropertySerializer
  -> IValueWriter
  -> FBinaryValueWriter
  -> IByteStream
  -> file or memory
```

## Object Graph Serialize

UObject graph 저장은 two-pass 방식을 사용한다.

### Save

```text
1. RootObject에서 hard reference graph 수집
2. 각 UObject에 ObjectId 할당
3. Objects 배열 저장
4. 각 object의 class, name, id 저장
5. 각 object의 properties 저장
6. TObjectPtr은 ObjectId로 저장
7. TSoftObjectPtr은 asset path로 저장
```

### Load

```text
1. Objects table 읽기
2. 모든 UObject를 먼저 생성
3. ObjectId -> UObject* resolver 등록
4. 각 object의 properties 로드
5. TObjectPtr은 resolver로 복원
6. PostLoad 호출
```

이 방식은 순환 참조에도 대응할 수 있다.

```text
ObjectA -> ObjectB
ObjectB -> ObjectA
```

모든 객체를 먼저 생성한 뒤 property를 로드하기 때문에 상호 참조를 안정적으로 복원할 수 있다.

## Property Filtering

직렬화 목적에 따라 property filter가 달라진다.

```text
Asset save
  include: Edit, SaveGame, default serializable
  skip: Transient

Duplicate
  include: Duplicate flag or default copyable
  skip: Transient, NonDuplicated

SaveGame
  include: SaveGame
  skip: Transient, EditorOnly

Network
  include: Replicated
  skip: non-replicated
```

filter는 serializer가 아니라 property traversal 단계에서 적용한다.

```cpp
bool ShouldSerializeProperty(
    const FProperty& Property,
    const FSerializationContext& Context);
```

## Error Handling

직렬화 실패는 조용히 무시하지 않는다. `FErrorReporter`를 통해 파일, object, property path를 함께 보고한다.

```text
SceneComponent.asset
Object: PlayerCamera
Property: Transform.RelativeLocation.X
Error: expected float, got string
```

reader는 현재 property path를 관리해야 한다.

```text
Root.Components[2].RelativeTransform.Location.X
```

이 정보는 codegen 에러와 마찬가지로 직렬화 디버깅에서 매우 중요하다.

## 구현 단계

### Phase 1: Core Interface

- `FSerializationContext`
- `IValueWriter`
- `IValueReader`
- `IByteStream` 또는 최소 `FArchive`
- `FErrorReporter`
- `FObjectReferenceResolver`

### Phase 2: Property Serializer

- `IPropertySerializer`
- primitive serializers
- enum serializer
- struct serializer
- array serializer
- object pointer serializer
- soft object pointer serializer

### Phase 3: Reflection Integration

- `FProperty::Serializer`
- generated property registration에서 serializer 연결
- `UObject::SaveProperties()`
- `UObject::LoadProperties()`
- property filtering

### Phase 4: Format Backend

- `FJsonValueWriter`
- `FJsonValueReader`
- `FBinaryTaggedValueWriter`
- `FBinaryTaggedValueReader`
- memory tree writer for tests

### Phase 5: Object Graph

- object graph collector
- object id assignment
- two-pass object load
- circular reference test
- missing class / missing property handling

### Phase 6: Advanced Policy

- custom versions
- save game filter
- network serializer
- editor copy-paste serializer
- diff-friendly asset output
- migration hooks

## 설계 원칙

- Archive는 저장 매체를 추상화할 뿐, 타입 정책을 알지 않는다.
- Property serializer가 타입별 정책의 소유자다.
- Format backend는 값과 구조를 실제 포맷으로 바꾸는 역할만 한다.
- Object reference policy는 serialization context와 resolver가 담당한다.
- Struct와 array는 항상 inner property를 재귀 처리한다.
- Soft reference와 hard reference는 GC 정책이 다르므로 serializer 단계에서 명확히 분리한다.
- Missing field는 목적에 따라 허용할 수 있지만, type mismatch는 error reporter에 남긴다.
- Binary format은 빠른 로딩용, JSON/structured format은 디버깅과 에디터 친화성을 위한 별도 backend로 본다.

## 결론

KnotEngine의 장기 방향은 다음과 같다.

```text
Archive 중심 직렬화
  -> Property Serializer 중심 직렬화
```

`FArchive` 하나로 모든 포맷과 정책을 해결하려고 하지 않는다. 대신 리플렉션 시스템처럼 타입별 serializer를 분산 등록하고, writer/reader backend를 통해 원하는 저장 포맷으로 변환한다.

이 구조는 처음 구현량은 조금 늘어나지만, 나중에 `TObjectPtr`, `TSoftObjectPtr`, `TArray<USTRUCT>`, save game, network replication, editor asset serialization이 들어와도 중앙 분기문과 archive 클래스가 폭발하지 않는다.
