# SimpleEngine Project

SimpleEngine 프로젝트는 Windows 환경에서 동작하는 고성능 3D 게임 엔진을 개발하는 것을 목적으로 한다. Direct3D 11 기반 렌더링, ImGui 기반 에디터 UI, PIR 지원, OBJ 메시 파이프라인을 갖추고 있다. Windows 전용이며 SSE/AVX SIMD를 활용하여 레이캐스트 및 컬링 성능을 최적화한다.

---

### Key Implementation (핵심 구현 사항)

엔진이 구동되기 위해 반드시 갖춰야 할 필수적인 기반 시스템입니다.

### 1. Core Framework & Memory System

- 엔진의 기초 체력이 되는 시스템입니다. 표준 라이브러리의 범위를 넘어 단편화를 최소화하기 위한 커스텀 할당자(Custom Allocator)와 풀링(Pooling) 시스템을 구축합니다. 또한, 엔진 내부에서 발생하는 모든 이벤트를 추적하는 로깅 시스템과 벡터, 행렬, 쿼터니언을 포함한 고성능 수학 라이브러리를 포함합니다.

### 2. Scene Graph & Basic Rendering

- 객체 간의 계층 구조를 관리하는 씬 그래프를 통해 변환(Transform) 데이터를 전파합니다. 그래픽 API를 추상화한 RHI(Render Hardware Interface)를 통해 기초적인 메시 출력, 재질(Material) 시스템, 그리고 카메라 가시성 판단(Culling)을 수행하는 기본적인 렌더링 파이프라인을 구현합니다.

### 3. Resource & Serialization System

- 디스크에 존재하는 에셋을 메모리로 로드하고 관리하는 시스템입니다. 프로퍼티 및 리플렉션 시스템을 도입하여 객체의 상태를 자동으로 직렬화(Serialization)하거나 역직렬화할 수 있는 구조를 갖춥니다. 이는 에셋 데이터베이스의 기초가 되며, 에디터와 엔진 간의 데이터 정합성을 유지하는 역할을 합니다.

### 4. Physics & Input System

- 물체 간의 충돌 감지(Collision Detection)와 기초적인 강체 역학 시뮬레이션을 담당합니다. 레이캐스팅(Raycasting)과 같은 필수 물리 연산을 지원하며, 키보드와 마우스 등 다양한 장치의 입력을 추상화하여 게임 로직에 전달하는 이벤트 중심의 입력 시스템을 구축합니다.

## Target Implementation (목표 구현 사항)

엔진의 완성도를 높이고 현대적인 게임 제작 환경을 지원하기 위한 고급 기술들입니다.
### 1. Advanced Rendering Architecture

- Hybrid Rendering: 물리 기반 렌더링(PBR)을 기초로 하여 Ray Marching 및 Ray Tracing 기술을 통합합니다.

- Modern Pipeline: 렌더 패스 관리를 유연하게 하는 렌더 그래프(Render Graph)와 드로우 콜 오버헤드를 획기적으로 줄이는 GPU Driven Rendering Pipeline을 구축합니다.

- Optimization: 대규모 씬 처리를 위한 동적 LOD(Level of Detail)와 가속 구조인 동적 BVH를 구현합니다.

### 2. Multi-threaded Architecture & Performance

- Thread Separation: 게임의 로직을 처리하는 스레드와 렌더링을 담당하는 스레드를 분리합니다. 두 스레드 간의 효율적인 데이터 전송을 위해 렌더 프록시(Render Proxy) 시스템을 도입합니다.

- Data-Oriented Design: CPU 캐시 효율을 극대화하기 위한 ECS(Entity Component System)를 구현하며, 멀티코어 환경을 십분 활용하기 위한 Job System을 구축합니다.

### 3. Workflow & Editor Tools

- VFS & Hot Reloading: 가상 파일 시스템(VFS)을 통해 리소스의 비동기 로드 및 종속성을 관리하며, 런타임 중에 에셋의 변경 사항을 즉각 반영하는 핫 리로딩 기능을 제공합니다.

- Editor Integration: 셰이더, 매터리얼, 메시 간의 복잡한 관계를 시각적으로 편집하고 디버깅할 수 있는 툴셋을 구축하여 개발 생산성을 높입니다.

---

### Roadmap

본 프로젝트는 단계별 성장을 지향합니다. 초기 단계에서는 안정적인 코어 시스템과 기초 렌더링에 집중하며, 점진적으로 고급 렌더링 기술과 데이터 중심 설계로 아키텍처를 확장할 계획입니다. 모든 과정에서 코드의 가독성과 아키텍처의 순수성을 유지하는 것을 원칙으로 합니다.

---
### Getting Started

본 프로젝트는 Visual Studio 2022 환경에서 C++20 표준을 사용하여 개발되었습니다.

- git clone --recursive 명령어를 사용하여 저장소와 필요한 서브모듈을 복제합니다.

- scripts 디렉토리의 Setup.bat을 실행하여 필수 SDK 및 종속성을 구성합니다.

- GenerateProjectFiles.bat을 실행하여 Visual Studio 솔루션 파일을 생성하고 프로젝트를 빌드합니다.