#pragma once

#include "Core/Debug.h"

#include <atomic>

// KnotEngine의 로그 매크로.
//
// assert와 달리 NDEBUG 구성(Development/Shipping)에서도 살아남는다.
// 셰이더를 잘못 저장하고 Development로 빌드했을 때 아무 메시지 없이 검은 화면만 나오는 상황을 막는다.
//
// 메시지는 std::format 문법을 쓰며 컴파일 타임에 검증된다.
//   KE_LOG(LogD3D11, Error, "Device 생성 실패. HRESULT=0x{:08X}", static_cast<uint32>(Result));
// 출력은 MSVC 진단 형식이라 Visual Studio 출력 창에서 더블클릭하면 해당 소스 줄로 이동한다.

#define KE_LOG(Category, Verbosity, ...) \
	FDebug::LogMessage(ELogVerbosity::Verbosity, #Category, __FILE__, __LINE__, __VA_ARGS__)

// 매 프레임 도는 경로용. 호출 지점마다 최초 1회만 출력한다.
#define KE_LOG_ONCE(Category, Verbosity, ...)                        \
	do                                                               \
	{                                                                \
		static std::atomic_bool bLogReported = false;                \
		if (!bLogReported.exchange(true, std::memory_order_relaxed)) \
		{                                                            \
			KE_LOG(Category, Verbosity, __VA_ARGS__);                \
		}                                                            \
	} while (false)
