#pragma once

#include "Core/Debug.h"

#if !defined(NDEBUG)
	#define check(expr)                                                    \
		do                                                                 \
		{                                                                  \
			if (!(expr))                                                   \
			{                                                              \
				FDebug::CheckFailed(#expr, __FILE__, __LINE__, __FUNCSIG__); \
				FDebug::Break();                                           \
			}                                                              \
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

	#define verifyf(expr, format, ...)                                                          \
		do                                                                                      \
		{                                                                                       \
			if (!(expr))                                                                        \
			{                                                                                   \
				FDebug::CheckFailed(#expr, __FILE__, __LINE__, __FUNCSIG__, format, __VA_ARGS__); \
				FDebug::Break();                                                                \
			}                                                                                   \
		} while (false)

	#define ensuref(expr, format, ...)                                                           \
		do                                                                                       \
		{                                                                                        \
			if (!(expr))                                                                         \
			{                                                                                    \
				FDebug::EnsureFailed(#expr, __FILE__, __LINE__, __FUNCSIG__, format, __VA_ARGS__); \
			}                                                                                    \
		} while (false)

	#define ensure(expr)                                                    \
		do                                                                  \
		{                                                                   \
			if (!(expr))                                                    \
			{                                                               \
				FDebug::EnsureFailed(#expr, __FILE__, __LINE__, __FUNCSIG__); \
			}                                                               \
		} while (false)

#else
	#define check(expr) ((void)0)
	#define checkf(expr, format, ...) ((void)0)

	#define verify(expr) ((void)(expr))
	#define verifyf(expr, format, ...) ((void)(expr))

	#define ensure(expr) ((void)(expr))
	#define ensuref(expr, format, ...) ((void)(expr))
#endif
