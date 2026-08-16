#pragma once

#include "Core/Debug.h"

// KnotEngine의 assertion 매크로. 의미는 언리얼 컨벤션을 참고하되 포맷 버전은 f 접미사로 통일한다.
//
// check: 복구 불가능한 불변식. 보고 후 디버거를 중단하고 프로세스를 끝낸다.
//        언리얼과 마찬가지로 실행이 이어지지 않으므로 뒤에 방어 코드를 붙이지 않는다.
// panic: 모든 빌드 구성에서 식을 평가하고 실패 시 프로세스를 끝낸다.
// verify: check와 같지만 Release에서도 식을 평가한다. 부수 효과가 있는 호출을 감쌀 때 쓴다.
// ensure: 실패를 보고하고 디버거를 중단하지만 실행은 계속한다. 조건을 bool로 반환한다.
//
// Release 빌드의 경우 check/checkf는 식을 평가하지 않는다. verify/ensure 계열은 평가하되 보고하지 않는다.

#define panicf(expr, ...)                                                                            \
	do                                                                                               \
	{                                                                                                \
		if (!(expr))                                                                                 \
		{                                                                                            \
			FDebug::PanicFailed({ #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__); \
			FDebug::Fatal();                                                                         \
		}                                                                                            \
	} while (false)

#define panic(expr) panicf(expr)

#if !defined(NDEBUG)

#define checkf(expr, ...)                                                                            \
	do                                                                                               \
	{                                                                                                \
		if (!(expr))                                                                                 \
		{                                                                                            \
			FDebug::CheckFailed({ #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__); \
			FDebug::Fatal();                                                                         \
		}                                                                                            \
	} while (false)

#define check(expr) checkf(expr)

#define verifyf(expr, ...) checkf(expr __VA_OPT__(, ) __VA_ARGS__)
#define verify(expr) checkf(expr)

// 실패할 때마다 보고한다.
#define ensureAlwaysf(expr, ...) \
	(!!(expr) ||                 \
	 (FDebug::EnsureFailed({ #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__) && (FDebug::BreakIfDebuggerPresent(), false)))

#define ensureAlways(expr) ensureAlwaysf(expr)

// 호출 지점당 최초 1회만 보고한다.
// 람다는 매크로가 전개된 위치마다 서로 다른 타입이 되므로 static도 호출 지점마다 따로 생긴다.
// __func__은 람다 밖에 두어야 바깥 함수 이름이 잡힌다.
#define ensuref(expr, ...)                                                       \
	(!!(expr) ||                                                                 \
	 (FDebug::EnsureFailedOnce(                                                  \
	      []() -> bool& { static bool bKnotEnsureReported = false; return bKnotEnsureReported; }(),                                                   \
	      { #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__) && \
	  (FDebug::BreakIfDebuggerPresent(), false)))

#define ensure(expr) ensuref(expr)

#else
#define check(expr) ((void)0)
#define checkf(expr, ...) ((void)0)

#define verify(expr) ((void)(expr))
#define verifyf(expr, ...) ((void)(expr))

#define ensure(expr) (!!(expr))
#define ensuref(expr, ...) (!!(expr))
#define ensureAlways(expr) (!!(expr))
#define ensureAlwaysf(expr, ...) (!!(expr))
#endif
