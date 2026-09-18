# Knot Engine Asset Pipeline Architecture

## 문서 목적

이 문서는 외부 `.glb` 파일과 DCC Tool의 데이터를 Knot Engine의 Mesh, Material, Texture, Animation과 Skeleton Asset으로 변환하는 파이프라인을 정의한다.

Asset UObject, CPU 데이터와 GPU Resource의 Runtime 구조는 [Asset-Architecture.md](Asset-Architecture.md)에서 다룬다.

## 설계 원칙

- `.glb`를 기본 3D Asset 교환 형식으로 사용한다.
- 일반 `.glb`와 Knot 확장이 포함된 `.glb`를 같은 Importer에서 처리한다.
- Blender 전용 기능은 Knot Engine Add-on이 표준 glTF 데이터와 Knot 확장으로 변환한다.
- Blender의 임의 Node Graph와 Control Rig를 Runtime에 그대로 저장하지 않는다.
- PBR은 표준 glTF Material로, NPR은 Knot Material Profile과 Parameter로 표현한다.
- 하나의 Source 파일에서 여러 Runtime Asset을 생성하되 각 Asset은 독립적으로 저장하고 참조한다.
- Runtime과 Shipping Build는 Blender, Python과 외부 Source 파일에 의존하지 않는다.
- 지원하지 않는 데이터는 묵시적으로 버리지 않고 경고 또는 오류로 보고한다.

## 전체 구조

```text
일반 DCC 또는 외부 Asset
└─ 표준 .glb
        │
Blender + Knot Engine Add-on
└─ 표준 glTF 데이터 + Knot 확장 .glb
        │
        ↓
FAssetImporter
├─ GLB Container와 Version 검증
├─ 표준 glTF 데이터 Import
├─ Knot 확장 Import
├─ 좌표계와 단위 변환
├─ Asset와 의존성 생성
└─ Import Metadata 기록
        ↓
Content
├─ Mesh.kasset
├─ Material.kasset
├─ Texture.kasset
├─ Skeleton.kasset
└─ Animation.kasset
```

외부에서는 하나의 `.glb`를 Import하지만, 엔진 내부에서는 용도와 수명이 다른 Asset으로 분리한다.

## 파일 형식의 역할

| 형식 | 역할 | Runtime 사용 |
|---|---|---|
| `.blend` | Blender 편집 원본과 Control Rig 보관 | 사용하지 않음 |
| `.glb` | DCC와 Knot Engine 사이의 교환 형식 | Editor Import에서만 사용 |
| `.kasset` | 검증되고 Cook된 Knot Runtime Asset | 사용 |

`.blend`는 Blender에서만 해석한다. Knot Engine은 Blender를 Headless로 실행해 원본을 읽지 않고, 사용자가 Export한 `.glb`를 Import한다.

`.glb`와 `.kasset`은 목적이 다르다. `.glb`는 교환과 재Import를 위한 Source이며, `.kasset`은 빠른 로드와 Runtime 검증을 위한 엔진 전용 Binary다.

## `.kasset` 직렬화 계층

`.kasset`의 Binary 입출력은 `FArchive` 계층으로 통일한다. Importer와 Loader가 각자 Byte Reader 또는 Writer를 구현하지 않고, Asset과 값 타입이 하나의 `operator<<`에서 저장 및 로드 순서를 공유한다.

```text
Editor Import
└─ Asset별 CPU Payload 구성
   └─ FMemoryWriter
      └─ TArray<uint8>
         └─ FAssetFileHeader + Asset Payload

Runtime Load
└─ .kasset 전체 Byte 로드
   └─ FMemoryReader
      ├─ Container Header 검증
      ├─ Asset Payload 역직렬화와 검증
      └─ UObject Asset 생성
```

### FArchive

`FArchive`는 저장 형식과 실제 저장 매체를 분리하는 공통 직렬화 Interface다.

- `Serialize()`는 연속된 Byte를 읽거나 쓴다.
- `IsLoading()`과 `IsSaving()`은 Archive의 방향을 나타낸다.
- `CanSerialize()`는 요청한 크기를 안전하게 처리할 수 있는지 확인한다.
- `HasError()`와 `SetError()`는 중간에 발생한 실패를 이후 단계까지 유지한다.
- 기본형, Enum과 단순 값 타입은 `operator<<`를 통해 직렬화한다.
- 복합 타입은 필드별 `operator<<`를 정의해 Padding과 구조체 Memory Layout에 파일 형식이 종속되지 않도록 한다.

`FString`은 최대 65,535 Byte로 제한한다. Loading 시에는 문자열 길이와 남은 Byte를 먼저 확인한 뒤 Memory를 할당하며, 실패한 문자열로 `FName`을 생성하지 않는다.

### FMemoryWriter와 FMemoryReader

`FMemoryWriter`는 호출자가 제공한 `TArray<uint8>`의 끝에 데이터를 추가한다. 음수 크기, null Source와 배열 최대 크기를 넘는 쓰기는 Archive Error로 처리한다. 생성 시 배열을 자동으로 비우지 않으므로 새 Payload를 만들 때는 호출자가 빈 배열을 전달한다.

`FMemoryReader`는 소유하지 않는 `std::span<const uint8>`를 순차적으로 읽는다. 원본 Byte 배열은 Reader보다 오래 살아 있어야 한다. 범위를 벗어난 읽기에서는 Offset과 Destination을 변경하지 않고 Archive Error를 설정한다.

Archive는 Byte 이동과 공통 오류 상태만 담당한다. Asset별 Count 제한, Enum 유효성, 참조 관계와 Index 범위는 데이터 의미를 아는 `FAssetBinaryLoader`가 검증한다.

### Asset Container와 Payload

모든 `.kasset`은 공통 `FAssetFileHeader` 뒤에 Asset별 Payload를 저장한다.

```text
FAssetFileHeader
├─ Magic
├─ ContainerVersion
├─ AssetType
├─ PayloadVersion
├─ PayloadSize
└─ FAssetId

Asset Payload
```

`ContainerVersion`은 공통 Container 구조의 변경을, `PayloadVersion`은 특정 Asset 형식의 변경을 나타낸다. Loader는 기대한 Asset Type과 Version을 확인하고, 선언된 Payload 크기가 실제 파일 크기와 정확히 일치하는지 검증한다. 이전 형식의 호환 Loader는 두지 않으며 형식이 변경되면 `.kasset`을 다시 생성한다.

`FAssetId`는 `.kasset`을 처음 생성할 때 발급되는 128-bit 영속 ID다. 이름 변경, 폴더 이동과 Reimport에서는 기존 ID를 유지하고 Asset 복제에서는 새 ID를 발급한다. `FAssetRegistry`는 ID와 현재 논리 경로를 양방향으로 인덱싱하므로 경로가 바뀌어도 참조를 복원할 수 있다.

Texture, Material과 Static Mesh의 Payload Header 및 `FShaderKey`도 같은 Archive 연산자를 사용한다. UObject Pointer와 GPU Handle은 저장하지 않으며 Asset 사이의 관계는 논리 경로가 아니라 `FAssetId`로 기록하고 로드 후 `FAssetManager`를 통해 해결한다. `UAsset` 계층은 영속 ID와 현재 표시 경로를 함께 보유하며, 일반 `UObject`의 런타임 UUID와 Asset ID는 서로 다른 수명과 용도로 유지한다.

### 로드 안전성

외부 파일의 값은 Memory 할당이나 UObject 생성 전에 검증한다.

- 문자열과 배열의 Count 및 Byte 크기
- Count와 Element Size 곱의 Overflow
- Texture Format, Color Space와 Material Enum 값
- Static Mesh의 Vertex, Index, Section과 Material Slot 범위
- Payload를 모두 읽은 뒤 남은 Byte가 없는지 여부

손상되거나 잘린 `.kasset`은 Crash나 부분 Asset 생성으로 이어지지 않아야 한다. `FMemoryReader`가 오류를 기록하면 `FAssetBinaryLoader`는 로드를 중단하며, 완전히 검증된 CPU 데이터만 UObject와 GPU Resource 생성 단계로 전달한다.

새로운 Asset 형식을 추가할 때는 Asset별 Payload 구조체와 명시적인 Archive 연산자를 먼저 정의한다. Asset 종류가 충분히 늘어 타입별 Dispatch가 반복되기 전까지는 Serializer Factory나 Registry 같은 추가 추상화는 도입하지 않는다.

## FAssetImporter

`FAssetImporter`는 Editor 전용 진입점이다.

```text
FAssetImporter
├─ Source 경로와 Import 설정 확인
├─ GLB JSON/Binary Chunk 해석
├─ Mesh, Material, Texture, Skin과 Animation 수집
├─ Knot 확장 해석
├─ Runtime Asset별 Payload 생성
├─ 임시 출력 검증
├─ 성공한 결과를 최종 경로에 반영
└─ FAssetRegistry 갱신
```

Importer는 표준 `.glb`만으로도 동작해야 한다. Knot 확장이 없으면 표준 PBR Material과 glTF Animation 규칙을 사용한다.

Import 실패 시 기존의 정상 `.kasset`을 덮어쓰지 않는다. 모든 임시 결과를 검증한 뒤 한 번에 반영한다.

## Blender Knot Engine Add-on

Blender Add-on은 Knot Engine이 지원하는 데이터 계약에 맞춰 `.glb`를 Export한다.

```text
Blender Mesh                 → glTF Mesh
Deformation Skeleton         → glTF Skin
Bake된 Action                → glTF Animation
Principled PBR               → glTF PBR Material
Knot NPR 설정                → KNOT_materials_npr
Clip / Event / Root Motion   → KNOT_animation_clips
Retarget 정보                → 향후 KNOT_skin_retarget
```

Add-on은 Knot Material Profile을 선택하고 Parameter와 Texture를 편집하는 UI를 제공한다. Blender Viewport용 Preview Node는 Add-on이 생성할 수 있지만, Exporter는 그 Node Graph를 다시 분석하지 않고 원래의 Profile과 Parameter를 기록한다.

일반 Blender Material은 표준 glTF PBR로 변환하거나 Texture로 Bake한다. 임의 Shader Node, Custom GLSL과 Render Pass는 자동으로 Knot Material이 되지 않는다.

## Asset별 Import 계약

| Asset | GLB 입력 | Knot Runtime 출력 |
|---|---|---|
| Mesh | Position, Normal, Tangent, UV, Vertex Color, Index와 Primitive | `UStaticMesh` 또는 `USkeletalMesh` |
| Material | glTF PBR Material과 Knot NPR 확장 | `UMaterialInstance`와 Engine Material Profile 참조 |
| Texture | GLB Image, Sampler와 Texture 용도 | `UTexture2D`와 Cooked Mip |
| Skeleton | Node 계층, Skin, Joint와 Inverse Bind Matrix | `USkeleton` |
| Animation | Node TRS, Morph Weight와 Knot Clip Metadata | `UAnimationSequence` |

Skin과 Morph Target이 없는 Mesh는 Static Mesh로 Import한다. Skin 또는 Morph Target이 있는 Mesh는 Skeletal Mesh 경로로 Import한다.

## PBR과 NPR Material

표준 PBR 속성은 glTF의 Metallic-Roughness Material로 저장한다. Knot Engine에서만 필요한 표현은 Vendor Extension으로 추가한다.

```text
glTF Material
├─ PBR Fallback
└─ KNOT_materials_npr
   ├─ Schema Version
   ├─ Material Profile
   ├─ Scalar / Vector Parameter
   ├─ NPR Texture Slot
   └─ Outline과 Render 설정
```

Material Profile은 엔진이 제공하는 Master Shader를 의미한다.

```text
StandardPBR
ToonSurface
CharacterFace
CharacterSkin
CharacterHair
CharacterEye
CharacterCloth
```

초기 구현에서는 실제로 필요한 Profile만 추가한다. 지원하지 않는 Blender Node를 위한 범용 Material Graph 변환은 구현하지 않는다.

Knot 확장을 모르는 Tool은 표준 PBR 부분을 Fallback으로 사용할 수 있다. Knot Engine은 확장을 읽어 정확한 NPR Material Instance를 생성한다.

## Texture

초기 모델 Import는 GLB에 포함된 PNG Texture를 지원한다. Texture 용도에 따라 Color Space와 GPU Format을 결정한다.

| 용도 | Color Space | 예시 Runtime Format |
|---|---|---|
| Base Color | sRGB | BC7 |
| Normal | Linear | BC5 |
| Mask / ORM / Face SDF | Linear | BC7 또는 BC1 |
| Ramp | Profile 계약에 따름 | BC7 |

JPEG는 손실이 허용되는 Color Texture에만 사용한다. Normal, Mask와 SDF에는 사용하지 않는다. HDR Texture와 편집 원본 PSD 같은 형식은 모델 Import와 분리된 Texture Importer가 필요해질 때 추가한다.

## Skeleton과 Animation

Blender의 Rig는 편집용 Control Rig와 Runtime Deformation Skeleton으로 구분한다.

```text
Blender Control Rig
├─ IK Controller
├─ Constraint
├─ Driver
└─ Custom Shape
        ↓ Bake
Deformation Skeleton
├─ Bone 계층
├─ Bind Pose
└─ Bone Transform Animation
        ↓ GLB Export
USkeleton + UAnimationSequence
```

Control Rig 자체는 Import하지 않는다. Constraint와 Driver의 결과는 Export 전에 Bone Transform 또는 Morph Weight Track으로 Bake한다.

glTF가 표현하지 않는 Loop, Root Motion, Animation Event와 Retarget Mapping은 Knot 확장 Metadata에 기록한다. Animation은 대상 Skeleton과의 호환성을 Import와 Load 시점에 검증한다.

## Sub-Asset과 참조

하나의 `.glb`는 여러 Runtime Asset을 생성할 수 있다.

```text
Character.glb
├─ SK_Character.kasset
├─ SKEL_Character.kasset
├─ MI_CharacterFace.kasset
├─ T_CharacterFaceSDF.kasset
├─ A_CharacterIdle.kasset
└─ A_CharacterWalk.kasset
```

Exporter는 Mesh, Material, Texture, Skeleton과 Animation에 안정적인 Sub-Asset ID를 기록한다. Object 순서나 glTF 배열 Index가 바뀌어도 같은 항목을 식별할 수 있어야 한다.

Importer는 다음 참조를 연결한다.

- Mesh에서 Material Slot
- Material Instance에서 Material Profile과 Texture
- Skeletal Mesh에서 Skeleton
- Animation에서 대상 Skeleton

## Import Metadata와 Reimport

Registry는 Source와 생성된 Sub-Asset의 관계를 Import Metadata로 관리한다.

```text
Source Asset ID
Source Path와 Content Hash
GLB / Knot Extension Schema Version
Importer Version
Import Settings Hash
생성된 Sub-Asset ID와 출력 경로
Sub-Asset별 Content Hash
```

파일 수정 시간은 빠른 변경 후보 판정에만 사용하고, 실제 변경 여부는 Content Hash로 확인한다.

Reimport는 전체 `.glb`를 임시 영역에서 다시 해석한 뒤 Sub-Asset별 Hash를 비교한다. 변경된 결과만 교체하되, 검증에 실패하면 기존 Asset을 그대로 유지한다.

## 좌표계와 단위

Importer는 glTF 데이터를 Knot Engine 규칙으로 변환한다.

- Knot Engine은 왼손 좌표계를 사용한다.
- +X는 Forward, +Y는 Right, +Z는 Up이다.
- 거리 단위는 센티미터다.
- 반사 변환이 발생하면 Triangle Winding과 Tangent Handedness를 함께 보정한다.
- Mesh, Bind Pose와 Animation Transform에 같은 변환 규칙을 적용한다.

좌표 변환을 Mesh에만 적용하고 Skeleton이나 Animation에 다르게 적용하면 Skinning이 깨지므로 Import 공통 계약으로 유지한다.

## 현재 구현과 목표

### 현재 구현

- `FAssetRegistry`의 `.glb`와 타입별 `.kasset` 검색
- `FAssetId`와 현재 경로의 양방향 Registry 조회
- Static Mesh → Material → Texture 참조의 영속 ID 직렬화
- GLB의 Static Mesh, Base Color Material과 embedded PNG Texture Import
- Static Mesh, Material, Texture2D `.kasset` 로드
- Base Color Texture용 sRGB BC7 Mip 생성
- glTF 좌표계와 미터 단위를 Knot 좌표계와 센티미터 단위로 변환
- Unlit `OpaquePS`, Alpha Mask용 `MaskedPS`, Alpha Blend용 `TranslucentPS` 렌더링
- Source보다 새 출력이 모두 존재하면 Import 생략
- Skin, Animation과 Morph Target Runtime Data는 아직 지원하지 않음

### 목표 순서

1. Normal, ORM 등 Texture 용도별 Color Space와 Material Parameter 확장
2. Import Metadata와 Hash 기반 Reimport 및 원자적 교체
3. Skeleton, Skinning과 Bake된 Animation Import
4. Blender Knot Engine Add-on과 안정적인 Sub-Asset ID
5. `KNOT_materials_npr`와 필요한 Material Profile

## 관련 문서

- [Asset-Architecture.md](Asset-Architecture.md)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [Editor-Architecture.md](Editor-Architecture.md)
- [Conventions.md](Conventions.md)

## 관련 파일

- [AssetImporter.h](../KnotEngine/Source/Editor/Asset/AssetImporter.h)
- [AssetId.h](../KnotEngine/Source/Engine/Asset/Asset/AssetId.h)
- [AssetRegistry.h](../KnotEngine/Source/Engine/Asset/AssetRegistry.h)
- [AssetManager.h](../KnotEngine/Source/Engine/Asset/AssetManager.h)
- [AssetBinaryLoader.h](../KnotEngine/Source/Engine/Asset/AssetBinaryLoader.h)
- [Archive.h](../KnotEngine/Source/Engine/Core/Archive.h)
- [MemoryArchive.h](../KnotEngine/Source/Engine/Core/MemoryArchive.h)
