#include "Core/Memory/Memory.h"

// 각 바이너리의 전역 연산자는 Engine.dll의 allocator와 통계를 공유한다.
_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(Size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t Size)
{
	return Allocate(Size);
}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(Size)
_VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t Size)
{
	return Allocate(Size);
}

void __CRTDECL operator delete(void* Memory) noexcept
{
	Free(Memory);
}

void __CRTDECL operator delete[](void* Memory) noexcept
{
	Free(Memory);
}

void __CRTDECL operator delete(void* Memory, size_t /*Size*/) noexcept
{
	Free(Memory);
}

void __CRTDECL operator delete[](void* Memory, size_t /*Size*/) noexcept
{
	Free(Memory);
}
