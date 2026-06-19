## Coding Conventions & Backgrounds

- 개발 환경은 C++20, Visual Studio 2022, v143 toolset, Windows SDK를 기준으로 한다.
- KnotEngine은 Windows 전용 엔진이며, Linux, macOS, console 등의 플랫폼 확장성을 목표로 하지 않는다.
- 현재 렌더링 백엔드는 Direct3D 11이지만, 장기적으로 렌더링 백엔드를 Direct 3D 12로 교체하는 것을 목표로 한다.
- 객체 생명주기 관리, 코딩 컨벤션, 리플렉션 시스템, GC 등 많은 시스템을 Unreal Engine을 참고해 구현할 예정이다.
- Debug, Shipping 빌드에 따른 표현식 평가 여부를 check(), verify(), ensure() 함수를 통해 관리한다.
