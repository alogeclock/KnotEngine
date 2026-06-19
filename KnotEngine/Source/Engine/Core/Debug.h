#pragma once

class FDebug
{
public:
	static void CheckFailed(const char* Expression, const char* File, int Line, const char* Function,
							const char* Format = nullptr, ...)
	{
		va_list Args;
		va_start(Args, Format);
		ReportFailure("Check", Expression, File, Line, Function, Format, Args);
		va_end(Args);
	}

	static void EnsureFailed(const char* Expression, const char* File, int Line, const char* Function,
							 const char* Format = nullptr, ...)
	{
		va_list Args;
		va_start(Args, Format);
		ReportFailure("Ensure", Expression, File, Line, Function, Format, Args);
		va_end(Args);
	}

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

	static void Break()
	{
		__debugbreak();
	}

private:
	static void ReportFailure(const char* Type, const char* Expression, const char* File, int Line,
							  const char* Function, const char* Format, va_list Args)
	{
		char Buffer[4096] = {};
		sprintf_s(Buffer, sizeof(Buffer), "[%s Failed]\n""  Expr: %s\n""  File: %s\n""  Line: %d\n""  Function: %s\n", Type, Expression, File, Line, Function);

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
