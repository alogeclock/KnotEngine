#pragma once

#include "Core/Debug.h"

#include <intrin.h>

// KnotEngine의 assertion 매크로. 기존 이름을 유지하고 포맷 버전은 f 접미사로 통일한다.
// - panic: 모든 빌드 구성에서 식을 평가하고 실패 시 프로세스를 끝낸다.
// - check: Debug와 Development에서만 식을 평가하고, 실패 시 보고 후 프로세스를 끝낸다.
// - verify: 모든 빌드 구성에서 식을 평가하며, Debug와 Development에서는 check와 같이 실패를 보고한다.

#if (defined(KNOT_BUILD_DEBUG) + defined(KNOT_BUILD_DEVELOPMENT) + defined(KNOT_BUILD_SHIPPING)) != 1
#error "Exactly one KnotEngine build configuration must be defined."
#endif

#define panicf(expr, ...)                                                                            \
	do                                                                                               \
	{                                                                                                \
		if (!(expr))                                                                                 \
		{                                                                                            \
			FDebug::PanicFailed({ #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__); \
			if (FDebug::IsDebuggerAttached())                                                        \
			{                                                                                        \
				__debugbreak();                                                                      \
			}                                                                                        \
			FDebug::Fatal();                                                                         \
		}                                                                                            \
	} while (false)
#define panic(expr) panicf(expr)

#if defined(KNOT_BUILD_DEBUG) || defined(KNOT_BUILD_DEVELOPMENT)

#define checkf(expr, ...)                                                                            \
	do                                                                                               \
	{                                                                                                \
		if (!(expr))                                                                                 \
		{                                                                                            \
			FDebug::CheckFailed({ #expr, __FILE__, __LINE__, __func__ } __VA_OPT__(, ) __VA_ARGS__); \
			if (FDebug::IsDebuggerAttached())                                                        \
			{                                                                                        \
				__debugbreak();                                                                      \
			}                                                                                        \
			FDebug::Fatal();                                                                         \
		}                                                                                            \
	} while (false)
#define check(expr) checkf(expr)

#define verifyf(expr, ...) checkf(expr __VA_OPT__(, ) __VA_ARGS__)
#define verify(expr) checkf(expr)

#else
#define check(expr) ((void)0)
#define checkf(expr, ...) ((void)0)

#define verify(expr) ((void)(expr))
#define verifyf(expr, ...) ((void)(expr))
#endif
