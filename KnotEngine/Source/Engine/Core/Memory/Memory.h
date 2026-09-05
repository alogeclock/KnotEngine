#pragma once

#include "Core/CoreTypes.h"
#include "EngineAPI.h"

#include <atomic>
#include <new>

// Engine.dll의 공통 allocator를 통과한 현재 할당량. 헤더 크기를 포함한다.
extern ENGINE_API std::atomic<uint64> TotalAllocationBytes;
extern ENGINE_API std::atomic<uint64> TotalAllocationCount;

ENGINE_API void* Allocate(size_t Size);
ENGINE_API void Free(void* Memory) noexcept;
