#include "Object.h"
#include "Object/Class.h"
#include "Object/ObjectStatics.h"

TArray<UObject*> GUObjectArray;
FUObjectManager GUObjectManager;

// 새 객체에 UUID와 배열 인덱스를 부여하고 전역 객체 배열에 등록한다.
UObject::UObject()
{
	UUID = ObjectStatics::GenerateUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

// 제거할 객체를 배열의 마지막 객체와 교환해 전역 객체 배열에서 상수 시간에 제거한다.
UObject::~UObject()
{
	check(!GUObjectArray.empty());
	check(InternalIndex < GUObjectArray.size());
	check(GUObjectArray[InternalIndex] == this);

	uint32 LastIndex = static_cast<uint32>(GUObjectArray.size() - 1);

	if (InternalIndex != LastIndex)
	{
		UObject* LastObject = GUObjectArray[LastIndex];
		GUObjectArray[InternalIndex] = LastObject;
		LastObject->InternalIndex = InternalIndex;
	}

	GUObjectArray.pop_back();
}

// 객체 생성 경로에서 연결한 실제 런타임 클래스를 반환한다.
UClass* UObject::GetClass() const
{
	panic(ClassPrivate);
	return ClassPrivate;
}

// 생성 중인 객체에 실제 런타임 클래스를 한 번만 연결한다.
void UObject::SetClass(UClass* InClass)
{
	panic(InClass);
	panic(ClassPrivate == nullptr);
	ClassPrivate = InClass;
}

// 실제 클래스가 지정한 클래스와 같거나 그 클래스를 상속하는지 확인한다.
bool UObject::IsA(const UClass* Class) const
{
	const UClass* ObjectClass = GetClass();
	return ObjectClass && ObjectClass->IsChildOf(Class);
}

void UObject::Serialize(FArchive& Ar)
{
	GetClass()->SerializeProperties(Ar, this);
}
