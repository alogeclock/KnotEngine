## Coordinate System & Units

- Knot Engine은 왼손 좌표계(Left-Handed Coordinate System)를 사용한다.
- +X축은 Forward, +Y축은 Right, +Z축은 Up을 의미한다.
- 월드 거리의 기본 단위는 센티미터(cm)이다.
- 회전 각도의 기본 단위는 도(degree)이다.
- 양의 회전 방향은 각 축의 규칙에 따라 정의한다.

## Coding Conventions & Backgrounds

- 개발 환경은 C++20, Visual Studio 2022, v143 toolset, Windows SDK를 기준으로 한다.
- KnotEngine은 Windows 전용 엔진이며, Linux, macOS, console 등의 플랫폼 확장성을 목표로 하지 않는다.
- 현재 렌더링 백엔드는 Direct3D 11이지만, 장기적으로 Direct 3D 12로 교체하는 것을 목표로 한다.
- 객체 생명주기 관리, 리플렉션 시스템, GC 등 많은 시스템을 Unreal Engine을 참고해 구현한다.
- Knot Engine의 Coding Convention은 Unreal Coding Convention을 따른다.
- Debug, Development, Shipping 빌드에 따른 표현식 평가 여부를 check(), verify(), ensure() 함수를 통해 관리한다.
- Unity Build의 이름 충돌을 막기 위해 헬퍼와 상수는 가능한 한 클래스나 함수 내부의 `static`으로 작성한다.
