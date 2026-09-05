# 모듈과 실행 구성

`KnotEngine.sln`은 다음 세 타깃을 포함한다.

| 타깃 | 산출물 | 책임 |
|---|---|---|
| Engine | Engine.dll | 기존 엔진 코드, RHI 계약, 렌더링 상위 코드, 객체와 리플렉션 |
| Renderer | Renderer.dll | D3D11 구현과 ImGui D3D11 백엔드 |
| Editor | Editor.exe | 창과 편집 UI를 연결하고 `UEditorEngine`을 실행 |

Renderer는 Engine에 정의된 RHI 계약을 구현한다. Editor는 Engine과 Renderer를 링크한다. Engine은 Renderer나 ImGui를 링크하지 않는다.

`RUNTIME_FILES`, `ALL_BUILD`, `ZERO_CHECK`는 CMake 보조 프로젝트이며 엔진 모듈이 아니다. 기본 시작 프로젝트는 Editor다. Client와 Server 실행 파일은 아직 추가하지 않는다.

## 초기화와 종료

DLL은 Windows 로더가 로드하며 `DllMain`에서는 엔진을 초기화하지 않는다. Editor의 `Launch()`가 기존 실행 순서를 연결한다.

1. 로그와 `FEngineLoop`를 준비한다.
2. EngineLoop가 Engine 타입을 등록하고 Editor의 Launch가 Editor 타입을 같은 Registry에 직접 등록한다.
3. 창과 `UEditorEngine`을 생성한다.
4. Renderer와 ImGui를 초기화하고 기존 Cube Component를 생성한다.
5. 기존 입력과 Tick 루프를 실행한다.
6. Editor 객체, UI, Renderer와 창을 역순으로 정리한다.

Actor Component, Scene Component, Primitive Component와 Mesh 렌더링 동작은 모듈 분리 전 구현을 유지한다.

## 데이터와 PCH

Contents, Settings, Shaders는 `RUNTIME_FILES`가 `Bin/x64/<Configuration>`으로 복사한다. 기존 `FPaths`의 탐색과 저장 경로 동작은 유지한다.

EnginePCH.h, RendererPCH.h, EditorPCH.h는 각 타깃의 PRIVATE PCH다. PCH 산출물은 타깃 사이에서 공유하지 않는다. DLL 경계를 통과하는 Engine과 Renderer 선언에 각각 `ENGINE_API`, `RENDERER_API`를 붙인다.

리플렉션 코드는 Engine과 Editor에 대해 별도로 생성한다. Editor 생성기는 Engine 타입 선언을 참조해 검증하며 Editor 소유 타입만 출력한다.

## 빌드 결과

`GenerateProjects.bat`으로 솔루션을 생성하고 `BuildEngine.bat`으로 빌드한다. 실행 파일과 DLL은 `KnotEngine/Bin/x64/<Configuration>`에, import library는 `KnotEngine/Intermediate/Lib/x64/<Configuration>`에 생성된다.
