#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

// 로그 상세도. Fatal은 두지 않는다. 치명적 실패는 Assert.h의 check 계열이 담당한다.
enum class ELogVerbosity : uint8_t
{
	Log,
	Warning,
	Error,
};

// 실패 지점의 소스 위치. 매크로만 채울 수 있으므로 집합 초기화로 넘긴다.
struct FDebugContext
{
	const char* Expression = nullptr;
	const char* File = nullptr;
	int Line = 0;
	const char* Function = nullptr;
};

// Visual Studio 및 파일 로그 출력, assertion 실패 보고, 디버거 중단을 담당하는 low-level 유틸리티 클래스.
// 출력은 MSVC 진단 형식 "파일(줄): 내용" 한 줄로 통일한다.
// Visual Studio 출력 창에서 더블클릭하면 해당 소스 줄로 이동하므로 절대 경로는 그대로 둔다.
class FDebug
{
public:
	static void Startup();
	static void Shutdown();
	static void Flush();
	static bool IsDebuggerAttached();

	// Check 계열 Assertion 실패 보고, 호출 직후 즉시 디버거를 중단한다.
	static void CheckFailed(const FDebugContext& Context);

	template <typename... TArgs>
	static void CheckFailed(const FDebugContext& Context, std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		char Message[MessageCapacity];
		ReportFailure("Check", Context, FormatToBuffer(Message, Format, std::forward<TArgs>(Args)...));
	}

	// Panic 실패 보고. check와 달리 모든 빌드 구성에서 호출된다.
	static void PanicFailed(const FDebugContext& Context);

	template <typename... TArgs>
	static void PanicFailed(const FDebugContext& Context, std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		char Message[MessageCapacity];
		ReportFailure("Panic", Context, FormatToBuffer(Message, Format, std::forward<TArgs>(Args)...));
	}

	// Ensure 계열 Assertion 실패 보고, 실패 기록 후에도 실행을 계속한다.
	// 이번 호출에서 실제로 보고했는지를 반환한다. 매크로가 이 값을 보고 호출 지점에서 중단점을 건다.
	static bool EnsureFailed(const FDebugContext& Context);

	template <typename... TArgs>
	static bool EnsureFailed(const FDebugContext& Context, std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		char Message[MessageCapacity];
		ReportFailure("Ensure", Context, FormatToBuffer(Message, Format, std::forward<TArgs>(Args)...));
		return true;
	}

	// 호출 지점당 최초 1회만 보고한다. bReported는 매크로가 호출 지점마다 따로 만들어 넘긴다.
	static bool EnsureFailedOnce(std::atomic_bool& bReported, const FDebugContext& Context);

	template <typename... TArgs>
	static bool EnsureFailedOnce(std::atomic_bool& bReported, const FDebugContext& Context, std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		if (bReported.exchange(true, std::memory_order_relaxed))
		{
			return false;
		}
		return EnsureFailed(Context, Format, std::forward<TArgs>(Args)...);
	}

	// 카테고리 로그. assert와 달리 NDEBUG 구성에서도 살아남는다.
	template <typename... TArgs>
	static void LogMessage(ELogVerbosity Verbosity, const char* Category, const char* File, int Line,
	                       std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		char Message[MessageCapacity];
		const std::string_view MessageView = FormatToBuffer(Message, Format, std::forward<TArgs>(Args)...);

		char Buffer[BufferCapacity];
		const auto Result = std::format_to_n(Buffer, BufferCapacity - 1, "{}({}): [{}] {}: {}\n",
		                                     File, Line, ToString(Verbosity), Category, MessageView);
		*Result.out = '\0';

		WriteMessage(Buffer, static_cast<size_t>(Result.out - Buffer), Verbosity == ELogVerbosity::Error);
	}

	// Visual Studio 출력 창과 로그 파일에 Formatted String을 출력한다.
	template <typename... TArgs>
	static void OutputDebugString(std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		char Buffer[BufferCapacity];
		const std::string_view Message = FormatToBuffer(Buffer, Format, std::forward<TArgs>(Args)...);
		WriteMessage(Buffer, Message.size(), false);
	}

	// 복구 불가능한 실패. 호출 지점에서 실패 보고와 디버거 중단을 마친 뒤 fail-fast로 끝낸다.
	// 이미 크래시 상태이므로 소멸자를 돌리지 않으며, OS가 크래시로 인식해 덤프 정책을 적용할 수 있다.
	[[noreturn]] static void Fatal();

private:
	struct FState;

	static constexpr size_t MessageCapacity = 2048;
	static constexpr size_t BufferCapacity = 4096;
	static FState& GetState();
	static void WriteMessage(const char* Message, size_t Length, bool bFlush);

	static const char* ToString(ELogVerbosity Verbosity);

	// 스택 버퍼에 직접 포맷한다. std::format과 달리 문자열을 반환하지 않으므로 할당이 없다.
	template <size_t Capacity, typename... TArgs>
	static std::string_view FormatToBuffer(char (&Buffer)[Capacity], std::format_string<TArgs...> Format, TArgs&&... Args)
	{
		const auto Result = std::format_to_n(Buffer, Capacity - 1, Format, std::forward<TArgs>(Args)...);
		*Result.out = '\0';
		return std::string_view(Buffer, static_cast<size_t>(Result.out - Buffer));
	}

	static void ReportFailure(const char* Type, const FDebugContext& Context, std::string_view Message);
};
