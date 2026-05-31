#include "Memory.h"

uint32 TotalAllocationBytes = 0;
uint32 TotalAllocationCount = 0;

// 엔진 전용 메모리 헤더, 16-bytes alignment를 유지한다.
struct FMemoryHeader
{
	size_t Size;
	size_t Padding;
};

// 사용자가 요청한 크기에 헤더를 추가하여 메모리를 할당한다.
void* operator new(size_t Size)
{
	if (Size == 0)
	{
		Size = 1;
	}

	const size_t TotalSize = Size + sizeof(FMemoryHeader);
	void* RawMemory = std::malloc(TotalSize);

	if (!RawMemory)
	{
		throw std::bad_alloc();
	}

	FMemoryHeader* Header = static_cast<FMemoryHeader*>(RawMemory);
	Header->Size = Size;

	TotalAllocationBytes += static_cast<uint32>(TotalSize);
	TotalAllocationCount++;

	return static_cast<void*>(Header + 1);
}

void* operator new[](size_t Size)
{
	return ::operator new(Size);
}

// 사용자가 넘겨준 주소에서 헤더를 찾고, Total Allocation Bytes에 반영한다.
void operator delete(void* Memory) noexcept
{
	if (!Memory)
		return;

	FMemoryHeader* Header = static_cast<FMemoryHeader*>(Memory) - 1;
	const size_t TotalSize = Header->Size + sizeof(FMemoryHeader);

	if (TotalAllocationBytes >= TotalSize)
	{
		TotalAllocationBytes -= static_cast<uint32>(TotalSize);
	}
	if (TotalAllocationCount > 0)
	{
		TotalAllocationCount--;
	}

	std::free(Header);
}

void operator delete[](void* Memory) noexcept
{
	::operator delete(Memory);
}

void operator delete(void* Memory, size_t /*Size*/) noexcept
{
	::operator delete(Memory);
}

void operator delete[](void* Memory, size_t /*Size*/) noexcept
{
	::operator delete(Memory);
}
