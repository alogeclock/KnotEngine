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
- 자원 생성 함수는 네이티브 API의 결과, 생성된 handle 및 사용 가능 상태를 함수 내부에서 검증한 뒤 성공한 자원만 반환한다.
- 초기화가 덜 된 자원이나 유효하지 않은 handle을 반환하고 호출자에게 검증 책임을 넘기지 않는다.
- Unity Build의 이름 충돌을 막기 위해 헬퍼와 상수는 namespace 대신 가능한 한 클래스나 함수 내부의 `static`으로 작성한다.

## Error Handling

- Debug, Development, Shipping 빌드에 따른 표현식 평가 여부를 panic(), check(), verify(), ensure() 함수를 통해 관리한다.
- 실패를 단순히 `nullptr`이나 null handle 반환으로 숨기지 않는다. 호출자가 복구할 수 있는 정상적인 실패만 nullable 반환으로 표현한다.
- `panic()`은 모든 빌드에서 반드시 평가하며 실패 시 즉시 종료해야 하는 조건에 사용한다.
- `check()`는 개발 중 내부 불변식 검증, `verify()`는 빌드 구성과 관계없이 실행해야 하는 표현식에 사용한다. 
- `ensure()`는 실패를 보고하되 실행을 계속할 수 있는 조건에 사용한다.