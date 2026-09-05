#include "Core/Memory/Memory.h"

#include <cstdlib>
#include <limits>

constinit std::atomic<uint64> TotalAllocationBytes = 0;
constinit std::atomic<uint64> TotalAllocationCount = 0;

// 엔진 전용 메모리 헤더, 16-bytes alignment를 유지한다.
struct FMemoryHeader
{
	size_t Size;
	size_t Padding;
};

// 사용자가 요청한 크기에 헤더를 추가하여 메모리를 할당한다.
void* Allocate(size_t Size)
{
	if (Size == 0)
	{
		Size = 1;
	}
	if (Size > (std::numeric_limits<size_t>::max)() - sizeof(FMemoryHeader))
	{
		throw std::bad_alloc();
	}
	const size_t TotalSize = Size + sizeof(FMemoryHeader);
	void* RawMemory = std::malloc(TotalSize);
	if (!RawMemory)
	{
		throw std::bad_alloc();
	}
	FMemoryHeader* Header = static_cast<FMemoryHeader*>(RawMemory);
	Header->Size = Size;
	TotalAllocationBytes.fetch_add(static_cast<uint64>(TotalSize), std::memory_order_relaxed);
	TotalAllocationCount.fetch_add(1, std::memory_order_relaxed);
	return static_cast<void*>(Header + 1);
}

// 사용자가 넘겨준 주소에서 헤더를 찾고 공통 통계에 반영한다.
void Free(void* Memory) noexcept
{
	if (!Memory)
	{
		return;
	}
	FMemoryHeader* Header = static_cast<FMemoryHeader*>(Memory) - 1;
	const size_t TotalSize = Header->Size + sizeof(FMemoryHeader);
	TotalAllocationBytes.fetch_sub(static_cast<uint64>(TotalSize), std::memory_order_relaxed);
	TotalAllocationCount.fetch_sub(1, std::memory_order_relaxed);
	std::free(Header);
}
