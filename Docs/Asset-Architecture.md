# Knot Engine Asset Architecture

## 문서 목적

이 문서는 Knot Engine이 Runtime Asset을 로드하고, UObject와 CPU 데이터 및 GPU Resource를 연결하는 구조를 정의한다.

외부 파일과 DCC Tool을 연결하는 Import 과정은 [Asset-Pipeline-Architecture.md](Asset-Pipeline-Architecture.md)에서 다룬다.

## 설계 원칙

- `FAssetRegistry`는 디스크의 Asset 메타데이터만 관리한다.
- `FAssetManager`는 로드된 Asset UObject를 캐시하고 수명을 유지한다.
- `FAssetBinaryLoader`는 `.kasset`을 검증하고 CPU 데이터를 역직렬화한다.
- Asset UObject, CPU 데이터와 GPU Resource의 소유권을 구분한다.
- GPU Resource는 실제 렌더링에 필요할 때 생성한다.
- Asset 종류마다 데이터와 검증 규칙을 명시적으로 구현한다.
- 외부 파일 형식과 DCC 전용 데이터는 Runtime 계층에 노출하지 않는다.

## 전체 구조

```text
Content/<Asset>.kasset
        ↓
FAssetBinaryLoader
├─ Header와 Version 검증
├─ Payload 역직렬화
└─ Asset UObject 생성
        ↓
FAssetManager Cache
        ↓
Asset을 사용하는 Component 또는 System
        ↓
CPU 데이터
        ↓ Render Command
Renderer Resource Cache
        ↓ 최초 사용 또는 Revision 변경
GPU Resource 생성·교체
```

Asset은 다음 세 계층으로 나뉜다.

| 계층 | 역할 | 예시 |
|---|---|---|
| Asset UObject | 영속 ID, 현재 논리 경로, Asset 타입과 참조 관계 관리 | `UStaticMesh`, `UMaterial`, `UMaterialInstance`, `UTexture2D` |
| CPU 데이터 | 역직렬화된 Runtime 데이터 보관 | Vertex, Index, Mip, Bone, Animation Track |
| GPU Resource | Renderer가 사용하는 Device Resource | Vertex Buffer, Texture, Constant Buffer |

모든 Asset이 GPU Resource를 가지는 것은 아니다. Skeleton과 Animation은 주로 CPU 데이터이며, 렌더링할 때 계산된 Skinning 결과만 GPU에 전달한다.

## Asset 검색과 로드

### FAssetRegistry

`FAssetRegistry`는 Runtime Asset 해석을 위해 Engine 계층에서 `Content`를 스캔하고 다음 메타데이터를 관리한다.

- 영속 `FAssetId`
- 논리 Asset 경로
- 실제 Source와 Binary 파일 경로
- Asset 타입
- Source와 Binary의 존재 여부

Registry는 UObject를 생성하거나 `.kasset` Payload를 읽지 않는다. Content Panel은 Registry의 결과만 표시한다.

### FAssetManager

`FAssetManager`는 `FAssetId`를 기준으로 로드된 Asset을 캐시한다. 경로 기반 API는 Registry에서 ID를 찾는 편의 진입점이며 실제 Cache와 Asset 간 참조는 경로에 종속되지 않는다.

```text
Load 요청
├─ Cache에 있음 → 기존 UObject 반환
└─ Cache에 없음
     ↓
   FAssetBinaryLoader
     ↓
   UObject 생성 및 Cache 등록
```

동일한 Asset을 사용하는 여러 Component는 하나의 UObject와 CPU 데이터를 공유한다. Manager는 보관 중인 `TObjectPtr`를 GC 참조 수집에 전달한다. 종료할 때는 Render Thread에서 Renderer의 Asset Resource Cache를 먼저 비운 뒤 Asset UObject를 파괴한다.

현재 `FAssetManager`와 `FAssetBinaryLoader`는 Static Mesh, Material, Texture2D를 지원하며 Asset별 Cache와 타입별 Payload 검증을 제공한다.

## Static Mesh

현재 구현된 Static Mesh의 소유 구조는 다음과 같다.

```text
FAssetManager
└─ TObjectPtr<UStaticMesh>
   ├─ AssetPath
   ├─ AssetId
	├─ Revision
	└─ FStaticMesh CPU Data
      ├─ FStaticMeshLOD[0]
      │  ├─ Vertex 배열
      │  ├─ Index 배열
	  │  ├─ Section 배열
	  │  └─ Local Bounds
      ├─ FStaticMeshLOD[1..]
      └─ 전체 Local Bounds

URenderer
└─ Static Mesh Resource Cache
   └─ FStaticMeshResource
	  └─ FStaticMeshLODResource[]
	     ├─ FMeshBuffer
	     └─ Section 복사본
```

`UStaticMesh`는 Asset 식별, CPU Mesh 데이터와 Revision을 소유한다. `FStaticMeshResource`는 Renderer Cache에서 CPU LOD에 대응하는 GPU `FMeshBuffer`와 Draw Section 복사본을 소유한다. 재임포트로 Revision이 변경되면 다음 Mesh Render Command에서 기존 Resource를 제자리에서 재생성한다.

`UStaticMeshComponent`는 `UStaticMesh`를 참조한다. Render Command 적용 시 CPU Mesh를 Resource로 변환한 뒤 `FStaticMeshSceneProxy`에는 Renderer 소유 `FStaticMeshResource`의 비소유 참조만 남는다. Draw Pass와 LOD 선택은 CPU Asset을 다시 조회하지 않는다.

현재 `.kasset`에는 Static Mesh Header와 LOD별 Vertex/Index Payload가 저장된다. Loader는 Magic, Version, Vertex Stride, 배열 범위와 Index 유효성을 검사한 뒤 `UStaticMesh`를 생성한다.

## Material과 Texture

Material은 Shader 자체와 인스턴스 값을 구분한다.

```text
UMaterialInterface
├─ UMaterial
│  ├─ FMaterial Shader와 Render State
│  └─ 기본 Parameter
└─ UMaterialInstance
   ├─ UMaterial 참조
   └─ Scalar / Vector / Texture Override

UTexture
└─ UTexture2D
   ├─ 크기, Format과 sRGB 정보
	└─ CPU Mip Payload

URenderer
└─ Texture Resource Cache
	└─ FTextureResource

URenderer
└─ Material Resource Cache
	└─ FMaterialResource
	   ├─ Pipeline과 Material Constant
	   └─ FTextureResource 참조와 Sampler
```

`UMaterial`은 Shader와 고정 Pipeline State의 실행 계약을 정의하고, `FMaterial`은 Content 소스 경로·Entry Point·Stage·Permutation ID로 구성된 `FShaderKey`를 저장한다. Shader Registry는 같은 Key의 GPU Shader를 최초 사용 시 생성하여 공유한다. `UMaterialInstance`는 Import된 색상, 수치와 Texture를 보관하며 부모 `UMaterial`의 Parameter를 이름으로 조회하고 Override가 없으면 기본값으로 돌아간다.

Texture는 다음 계층으로 관리한다.

Texture의 원본 PNG 같은 Source 파일은 Editor Import 입력이다. Runtime은 플랫폼에 맞게 변환된 `.kasset` Mip Payload를 읽는다. `UTexture2D`는 CPU Mip과 Revision을 소유하고 Renderer가 최초 Material Resource 생성 시 GPU `FTextureResource`를 생성한다. Material은 Texture UObject나 복사된 GPU Handle 대신 Renderer가 안정적으로 소유하는 `FTextureResource`를 참조한다.

## Skeletal Mesh와 Skeleton

Skeletal Mesh와 Skeleton은 별도 Asset이다.

```text
USkeletalMesh
├─ Vertex / Index / Section
├─ Joint Index / Weight
├─ Bind 정보
├─ USkeleton 참조
└─ Material Slot

USkeleton
├─ Bone 이름
├─ Parent 계층
└─ Reference Pose
```

여러 Skeletal Mesh와 Animation이 하나의 `USkeleton`을 공유할 수 있어야 한다. Skeletal Mesh는 Vertex/Index Buffer를 가지지만 Skeleton 자체는 지속적인 GPU Mesh Resource를 소유하지 않는다.

매 프레임 계산한 Bone Transform Palette는 Scene 또는 Animation 결과에서 만들어 Renderer에 전달한다. 향후 GPU Skinning을 구현해도 `USkeleton`의 계층 데이터와 프레임별 GPU Palette는 별도 수명으로 관리한다.

## Animation

Animation은 대상 Skeleton을 기준으로 재생 가능한 Runtime Track을 보관한다.

```text
UAnimationSequence
├─ USkeleton 참조
├─ 재생 길이와 Sampling 정보
├─ Bone Transform Track
├─ Morph Weight Track
└─ Event와 Root Motion 정보
```

Animation Asset은 일반적으로 지속적인 GPU Resource를 소유하지 않는다. Animation System이 CPU에서 Pose를 평가하고, 최종 Bone Transform만 Skinning 경로에 전달한다.

DCC의 Constraint, Driver와 Control Rig는 Animation Asset에 저장하지 않는다. Import 전에 평가하고 Runtime이 지원하는 Bone Transform과 Morph Weight Track으로 Bake한다.

## Asset 참조 관계

```text
UStaticMeshComponent
└─ UStaticMesh
   └─ Material Slot 또는 Component Override
      └─ UMaterialInstance
         ├─ UMaterial
         └─ UTexture2D[]

USkeletalMeshComponent
├─ USkeletalMesh
│  └─ USkeleton
└─ UAnimationSequence
   └─ 같은 USkeleton
```

Import Source 하나가 여러 Runtime Asset을 생성해도 Asset은 서로 명시적으로 참조한다. Source Bundle은 생성 관계를 기록하지만 Runtime Asset의 소유자가 되지 않는다.

Asset 간 참조와 리플렉션으로 저장되는 Asset UObject 참조는 `FAssetId`로 직렬화한다. Registry는 ID를 현재 경로로 해석하므로 파일명이나 폴더가 바뀌어도 참조가 유지된다. 일반 UObject의 UUID는 현재 실행의 객체 식별자로만 사용하고 영속 Asset 참조와 구분한다.

Skeleton 호환성, Material Parameter 타입과 Texture 용도처럼 참조가 성립하기 위한 조건은 Asset을 로드하거나 연결할 때 검증한다.

## CPU와 GPU Resource 수명

바이너리 로드는 UObject와 CPU 데이터까지만 생성한다. GPU Resource는 Render Thread에서 Renderer Cache가 최초 사용 시 초기화한다.

```text
.kasset Load
    ↓
UObject + CPU Data
	↓ Render Command
Renderer::GetOrCreate Resource
	↓
GPU Resource
```

Static Mesh, Texture, Material의 GPU 대응 객체는 각각 `FStaticMeshResource`, `FTextureResource`, `FMaterialResource`로 통일한다. 세 Cache는 모두 영속 `FAssetId`를 Key로 사용하고 Resource가 기록한 `SourceRevision`과 Asset Revision이 다를 때 같은 Resource 인스턴스를 갱신한다. GPU Resource의 생성과 해제는 Render Thread에서만 수행한다.

Resource Cache는 Asset 주소를 Key로 사용하지 않으며 Resource도 UObject를 장기 소유하지 않는다. 현재 최초 생성·갱신 명령을 적용하는 순간에는 `FAssetManager`가 수명을 보장하는 CPU Asset을 읽지만, Scene Proxy와 이후 Draw Pass에는 Renderer Resource 참조만 유지한다. 기본 Material과 White Texture는 유효하지 않은 Asset ID를 Cache Key로 사용하는 대신 Renderer의 명시적인 기본 Resource로 소유한다.

GPU Handle은 Asset UObject의 생존만으로 안전해지는 것이 아니다. Renderer Cache는 Asset UObject와 Render Device보다 먼저 비워야 하며, D3D12 도입 후에는 GPU 작업 완료를 확인한 뒤 지연 해제해야 한다.

CPU 데이터를 GPU 업로드 뒤 유지할지는 Asset 타입과 Editor 기능에 따라 결정한다. 충돌 생성, 재업로드 또는 Editor 미리보기에 필요하면 유지하고, 큰 Runtime Asset에서 필요하지 않다면 Cook 설정으로 제거할 수 있다.

## 현재 구현 상태

| 기능 | 상태 |
|---|---|
| `FAssetRegistry`의 Content 스캔과 메타데이터 | 구현 |
| `FAssetId` 기반 Cache와 이동에 안전한 Asset 참조 | 구현 |
| Static Mesh `.kasset` 검증과 역직렬화 | 구현 |
| `UStaticMesh`와 `FAssetManager` Cache | 구현 |
| Static Mesh CPU LOD와 Bounds | 구현 |
| 최초 가시 Draw의 GPU Buffer 생성 | 구현 |
| Material과 Texture UObject 및 Render Resource 계층 | 구현 |
| Material과 Texture `.kasset` 역직렬화 | 구현 |
| Skeletal Mesh와 Skeleton Asset | 미구현 |
| Animation Asset | 미구현 |
| Asset 의존성과 영속 ID | 미구현 |

## 관련 문서

- [Asset-Pipeline-Architecture.md](Asset-Pipeline-Architecture.md)
- [Rendering-Architecture.md](Rendering-Architecture.md)
- [Editor-Architecture.md](Editor-Architecture.md)
- [Reflection-Architecture.md](Reflection-Architecture.md)

## 관련 파일

- [AssetManager.h](../KnotEngine/Source/Engine/Asset/AssetManager.h)
- [AssetId.h](../KnotEngine/Source/Engine/Asset/Asset/AssetId.h)
- [AssetBinaryLoader.h](../KnotEngine/Source/Engine/Asset/AssetBinaryLoader.h)
- [StaticMesh.h](../KnotEngine/Source/Engine/Asset/Mesh/StaticMesh.h)
- [StaticMeshResource.h](../KnotEngine/Source/Engine/Render/Resource/Mesh/StaticMeshResource.h)
- [GeometryMesh.h](../KnotEngine/Source/Engine/Render/Resource/Mesh/GeometryMesh.h)
- [MaterialInterface.h](../KnotEngine/Source/Engine/Asset/Material/MaterialInterface.h)
- [Texture2D.h](../KnotEngine/Source/Engine/Asset/Texture/Texture2D.h)
- [Material.h](../KnotEngine/Source/Engine/Asset/Material/Material.h)
- [MaterialResource.h](../KnotEngine/Source/Engine/Render/Resource/MaterialResource.h)
- [TextureResource.h](../KnotEngine/Source/Engine/Render/Resource/TextureResource.h)
- [AssetRegistry.h](../KnotEngine/Source/Engine/Asset/AssetRegistry.h)
