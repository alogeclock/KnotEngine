# Knot Engine

> **This project is in its early stages.**
> Core systems are still being built, and many features described below are planned or in progress.

A 3D game engine project for Windows, built with Direct3D 11, ImGui, and modern C++20.

---

## Overview

KnotEngine is a personal project aimed at building a fully-featured 3D game engine from scratch. The focus is on clean architecture, performance, and progressive complexity — starting from stable core systems and expanding toward advanced rendering and data-oriented design.

**Platform:** Windows only  
**Renderer:** Direct3D 11  
**Language:** C++20 (Visual Studio 2022)

### Coordinate System and Units

- Knot Engine uses a left-handed coordinate system.
- The +X axis represents Forward, +Y represents Right, and +Z represents Up.
- The default unit of world distance is the centimeter (cm).
- The default unit of rotation angle is the degree.
- Positive rotation is defined according to the convention of each axis.

---

## Features

### Key Implementation (Core Goals)

These are the foundational systems the engine must have to run.

- **Core Framework & Memory System** — Custom allocators, pooling, logging, and a high-performance math library (vectors, matrices, quaternions)
- **Scene Graph & Basic Rendering** — Transform hierarchy, RHI abstraction, mesh rendering, material system, and frustum culling
- **Resource & Serialization System** — Asset loading/management, property reflection, and serialization for editor-engine data consistency
- **Physics & Input System** — Collision detection, basic rigid body simulation, raycasting, and an event-driven input abstraction layer

### Target Implementation (Stretch Goals)

Advanced features planned for future milestones.

- **Advanced Rendering** — PBR, Ray Marching, Ray Tracing, Render Graph, GPU Driven Rendering, dynamic LOD, and BVH acceleration structures
- **DirectX 12 Backend Migration** — Replace the rendering backend with DirectX 12 for modern graphics API support
- **Multi-threaded Architecture** — Separated game/render threads with Render Proxy system, ECS, and a Job System for multi-core utilization
- **Workflow & Editor Tools** — Virtual File System (VFS), hot reloading, and a visual editor for shaders, materials, and meshes

Code readability and architectural integrity are maintained as first-class priorities throughout all phases.

---

## Getting Started

**Requirements:** Visual Studio 2022 with the v143 toolset, Windows SDK

CMake, Python, vcpkg, and third-party packages are prepared automatically by the project scripts.

```bash
# 1. Clone Engine
git clone https://github.com/your-repo/KnotEngine.git
cd KnotEngine

# 2. Prepare dependencies and generate Visual Studio project files
GenerateProjects.bat

# 3. Build Development (default), Debug, Shipping, or all configurations
BuildEngine.bat
BuildEngine.bat Debug
BuildEngine.bat Shipping
BuildEngine.bat All
```

Open the generated root solution at `KnotEngine.sln` in Visual Studio 2022 and build.

Run `GenerateProjects.bat` for initial setup and after adding or removing source files. Use `BuildEngine.bat` for regular builds without regenerating the Visual Studio solution.

Generated tools, package files, and intermediate build files are kept under `KnotEngine/Intermediate`. Build outputs are written to `KnotEngine/Bin/x64/<Configuration>`.

Logs are written to the Visual Studio Output window and `Saved/Logs/KnotEngine.log`.

---

## License

Knot Engine is released under the [MIT License](LICENSE).

---

# Knot Engine

> **이 프로젝트는 아직 초기 단계입니다.**
> 핵심 시스템을 구축하는 중이며, 아래에 설명된 많은 기능은 계획 중이거나 개발 중입니다.

Direct3D 11, ImGui, 최신 C++20으로 개발하는 Windows용 3D 게임 엔진 프로젝트입니다.

---

## 개요

KnotEngine은 처음부터 완전한 3D 게임 엔진을 만들어 보는 개인 프로젝트입니다. 안정적인 핵심 시스템에서 출발해 고급 렌더링과 데이터 지향 설계로 확장해 나가며, 깔끔한 아키텍처, 성능, 점진적인 복잡도 증가에 초점을 둡니다.

**플랫폼:** Windows 전용  
**렌더러:** Direct3D 11  
**언어:** C++20 (Visual Studio 2022)

### 좌표계와 단위

- Knot Engine은 왼손 좌표계(Left-Handed Coordinate System)를 사용한다.
- +X축은 Forward, +Y축은 Right, +Z축은 Up을 의미한다.
- 월드 거리의 기본 단위는 센티미터(cm)이다.
- 회전 각도의 기본 단위는 도(degree)이다.
- 양의 회전 방향은 각 축의 규칙에 따라 정의한다.

---

## 기능

### 핵심 구현 항목 (Core Goals)

엔진이 실행되기 위해 반드시 갖춰야 하는 기반 시스템입니다.

- **코어 프레임워크와 메모리 시스템** — 커스텀 할당자, 풀링, 로깅, 벡터/행렬/쿼터니언을 포함한 고성능 수학 라이브러리
- **씬 그래프와 기본 렌더링** — 트랜스폼 계층 구조, RHI 추상화, 메시 렌더링, 머티리얼 시스템, 프러스텀 컬링
- **리소스와 직렬화 시스템** — 에셋 로딩/관리, 프로퍼티 리플렉션, 에디터와 엔진 데이터 일관성을 위한 직렬화
- **물리와 입력 시스템** — 충돌 감지, 기본 강체 시뮬레이션, 레이캐스팅, 이벤트 기반 입력 추상화 레이어

### 목표 구현 항목 (Stretch Goals)

향후 마일스톤에서 계획 중인 고급 기능입니다.

- **고급 렌더링** — PBR, Ray Marching, Ray Tracing, Render Graph, GPU Driven Rendering, 동적 LOD, BVH 가속 구조
- **DirectX12 백엔드 교체** — 최신 그래픽스 API 지원을 위해 렌더링 백엔드를 DirectX12로 교체
- **멀티스레드 아키텍처** — Render Proxy 시스템을 갖춘 게임/렌더 스레드 분리, ECS, 멀티코어 활용을 위한 Job System
- **워크플로우와 에디터 도구** — Virtual File System (VFS), 핫 리로딩, 셰이더/머티리얼/메시를 위한 비주얼 에디터

모든 단계에서 코드 가독성과 아키텍처 무결성을 최우선 가치로 유지합니다.

---

## 시작하기

**요구 사항:** Visual Studio 2022 v143 툴체인, Windows SDK

CMake, Python, vcpkg, 서드파티 패키지는 프로젝트 스크립트가 자동으로 준비합니다.

```bash
# 1. 엔진 클론
git clone https://github.com/your-repo/KnotEngine.git
cd KnotEngine

# 2. 의존성 준비 및 Visual Studio 프로젝트 파일 생성
GenerateProjects.bat

# 3. Development(기본), Debug, Shipping 또는 전체 구성 빌드
BuildEngine.bat
BuildEngine.bat Debug
BuildEngine.bat Shipping
BuildEngine.bat All
```

루트에 생성된 `KnotEngine.sln` 파일을 Visual Studio 2022에서 열고 빌드합니다.

`GenerateProjects.bat`는 최초 설정과 소스 파일 추가·삭제 후에 실행합니다. 일반 빌드는 Visual Studio 솔루션을 재생성하지 않는 `BuildEngine.bat`를 사용합니다.

생성된 도구, 패키지 파일, 중간 빌드 파일은 `KnotEngine/Intermediate` 아래에 정리됩니다. 빌드 결과물은 `KnotEngine/Bin/x64/<Configuration>`에 생성됩니다.

로그는 Visual Studio 출력 창과 `Saved/Logs/KnotEngine.log`에 기록됩니다.

---

## 라이선스

Knot Engine은 [MIT 라이선스](LICENSE)로 배포됩니다.
