#include "Object/ObjectInitializer.h"

#include "Core/Assert.h"

// 스레드별 UObject 생성 컨텍스트의 최상단 저장소를 반환한다.
FObjectInitializer*& FObjectInitializer::GetCurrentStorage()
{
	static thread_local FObjectInitializer* Current = nullptr;
	return Current;
}

// 현재 스레드에서 실행 중인 UObject 생성 컨텍스트를 반환한다.
FObjectInitializer* FObjectInitializer::GetCurrent()
{
	return GetCurrentStorage();
}

// 현재 생성 컨텍스트를 중첩 가능한 스택의 최상단으로 설정한다.
FObjectInitializer::FObjectInitializer(UClass& InClass, UObject* InOuter, FName InName, 
	UObject* InTemplate, EObjectFlags InFlags, FObjectInstancingContext* InInstancingContext)
    : Previous(GetCurrentStorage()), Class(InClass), Outer(InOuter),
      Template(InTemplate), Name(std::move(InName)), Flags(InFlags), InstancingContext(InInstancingContext)
{
	GetCurrentStorage() = this;
}

// 생성 컨텍스트가 정확한 역순으로 종료됐는지 검증하고 이전 컨텍스트를 복원한다.
FObjectInitializer::~FObjectInitializer()
{
	check(GetCurrentStorage() == this);
	GetCurrentStorage() = Previous;
}

// UObject 생성자 안에서 사용할 현재 생성 컨텍스트를 반환한다.
FObjectInitializer& FObjectInitializer::Get()
{
	check(GetCurrentStorage());
	return *GetCurrentStorage();
}
