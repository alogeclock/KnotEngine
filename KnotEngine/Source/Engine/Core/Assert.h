#pragma once

#include "Core/Debug.h"

// KnotEngine의 assertion 매크로.
  
// [Debug] 조건 실패 시 FDebug로 실패 정보를 출력한다.
// - check/verify: 실패 후 디버거를 중단한다. 
// - ensure: 실패를 보고한 뒤 실행을 계속한다.

// [Release]
// - check/checkf: 식을 평가하지 않는다.
// - verify/verifyf: 식은 평가하지만 실패해도 중단하지 않는다.
// - ensure/ensuref: 식은 평가하지만 실패해도 보고하지 않는다.

#if !defined(NDEBUG)
	#define check(expr)                                                      \
		do                                                                   \
		{                                                                    \
			if (!(expr))                                                     \
			{                                                                \
				FDebug::CheckFailed(#expr, __FILE__, __LINE__, __FUNCSIG__); \
				FDebug::Break();                                             \
			}                                                                \
		} while (false)

	#define checkf(expr, format, ...)                                                             \
		do                                                                                        \
		{                                                                                         \
			if (!(expr))                                                                          \
			{                                                                                     \
				FDebug::CheckFailed(#expr, __FILE__, __LINE__, __FUNCSIG__, format, __VA_ARGS__); \
				FDebug::Break();                                                                  \
			}                                                                                     \
		} while (false)

	#define verify(expr) check(expr)

	#define verifyf(expr, format, ...)                                                            \
		do                                                                                        \
		{                                                                                         \
			if (!(expr))                                                                          \
			{                                                                                     \
				FDebug::CheckFailed(#expr, __FILE__, __LINE__, __FUNCSIG__, format, __VA_ARGS__); \
				FDebug::Break();                                                                  \
			}                                                                                     \
		} while (false)

	#define ensuref(expr, format, ...)                                                             \
		do                                                                                         \
		{                                                                                          \
			if (!(expr))                                                                           \
			{                                                                                      \
				FDebug::EnsureFailed(#expr, __FILE__, __LINE__, __FUNCSIG__, format, __VA_ARGS__); \
			}                                                                                      \
		} while (false)

	#define ensure(expr)                                                      \
		do                                                                    \
		{                                                                     \
			if (!(expr))                                                      \
			{                                                                 \
				FDebug::EnsureFailed(#expr, __FILE__, __LINE__, __FUNCSIG__); \
			}                                                                 \
		} while (false)

#else
	#define check(expr) ((void)0)
	#define checkf(expr, format, ...) ((void)0)

	#define verify(expr) ((void)(expr))
	#define verifyf(expr, format, ...) ((void)(expr))

	#define ensure(expr) ((void)(expr))
	#define ensuref(expr, format, ...) ((void)(expr))
#endif
