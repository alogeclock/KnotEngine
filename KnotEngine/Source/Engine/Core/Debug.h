#pragma once

#include <Windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <intrin.h>

// 디버그 출력, assertion 실패 보고, 디버거 중단을 담당하는 low-level 유틸리티 클래스.
class FDebug
{
public:
    // Check 계열 Assertion 실패 보고, 호출 직후 즉시 디버거 중단.
    static void CheckFailed(const char* Expression, const char* File, int Line, const char* Function,
                            const char* Format = nullptr, ...)
    {
        va_list Args;
        va_start(Args, Format);
        ReportFailure("Check", Expression, File, Line, Function, Format, Args);
        va_end(Args);
    }

    // Ensure 계열 Assertion 실패 보고, 실패 기록 후에도 실행을 계속.
    static void EnsureFailed(const char* Expression, const char* File, int Line, const char* Function,
                             const char* Format = nullptr, ...)
    {
        va_list Args;
        va_start(Args, Format);
        ReportFailure("Ensure", Expression, File, Line, Function, Format, Args);
        va_end(Args);
    }

    // Visual Studio 출력 창에 Formatted String 출력.
    static void OutputDebugString(const char* Format, ...)
    {
        if (!Format)
        {
            return;
        }

        char Buffer[2048] = {};
        va_list Args;
        va_start(Args, Format);
        vsnprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, Format, Args);
        va_end(Args);

        OutputDebugStringA(Buffer);
    }

    // 현재 위치에서 디버거 중단점 발생
    static void Break()
    {
        __debugbreak();
    }

private:
    static void ReportFailure(const char* Type, const char* Expression, const char* File, int Line,
                              const char* Function, const char* Format, va_list Args)
    {
        char Buffer[4096] = {};
        sprintf_s(Buffer, sizeof(Buffer), "[%s Failed]\n"
                                          "  Expr: %s\n"
                                          "  File: %s\n"
                                          "  Line: %d\n"
                                          "  Function: %s\n",
                  Type, Expression, File, Line, Function);

        if (Format)
        {
            strcat_s(Buffer, sizeof(Buffer), "  Message: ");

            const size_t Length = std::strlen(Buffer);
            if (Length < sizeof(Buffer))
            {
                vsnprintf_s(Buffer + Length, sizeof(Buffer) - Length, _TRUNCATE, Format, Args);
            }

            strcat_s(Buffer, sizeof(Buffer), "\n");
        }

        OutputDebugStringA(Buffer);
    }
};
