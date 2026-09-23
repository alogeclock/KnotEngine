#include "Object.h"
#include "Object/Reflection/Class.h"
#include "Object/ObjectInstancingContext.h"

#include <algorithm>

TArray<UObject*> GUObjectArray;
FUObjectManager GUObjectManager;
uint32 UObject::NextUUID = 1;

// CDO 또는 명시적 Template을 기준으로 객체와 생성자 Default Subobject의 반영 프로퍼티를 초기화한다.
UObject* FUObjectManager::NewObject(UClass& Class, UObject* Outer, FName Name, UObject* Template, EObjectFlags Flags, FObjectInstancingContext* ExistingContext)
{
	panic(Class.CanCreateObject());
	if (!Template && (Flags & EObjectFlags::ClassDefaultObject) == EObjectFlags::None)
	{
		Template = Class.GetDefaultObject();
	}

	FObjectInstancingContext LocalContext;
	FObjectInstancingContext& Context = ExistingContext ? *ExistingContext : LocalContext;
	FObjectInitializer Initializer(Class, Outer, std::move(Name), Template, Flags, &Context);
	UObject* Object = Class.ConstructObject(Outer, Initializer.GetName(), Template, Flags);
	if (Template)
	{
		Context.Add(*Template, *Object);
	}

	if (!ExistingContext)
	{
		for (const auto& [Source, Destination] : Context.GetObjects())
		{
			Source->GetClass()->CopyProperties(Destination, Source, Context);
		}
		for (const auto& [Source, Destination] : Context.GetObjects())
		{
			Destination->PostInitProperties();
		}
	}
	return Object;
}

// 원본 객체와 생성자 Default Subobject를 한 Instancing Context에 만들고 참조를 복제 대상 객체로 치환한다.
UObject* FUObjectManager::DuplicateObject(const UObject& Source, UObject* NewOuter, FName NewName)
{
	UClass& Class = *Source.GetClass();
	panic(Class.CanCreateObject());

	FObjectInstancingContext Context;
	FObjectInitializer Initializer(Class, NewOuter, std::move(NewName), const_cast<UObject*>(&Source), EObjectFlags::None, &Context);
	UObject* Duplicate = Class.ConstructObject(NewOuter, Initializer.GetName(), const_cast<UObject*>(&Source), EObjectFlags::None);
	Context.Add(Source, *Duplicate);
	for (const auto& [ContextSource, ContextDestination] : Context.GetObjects())
	{
		ContextSource->GetClass()->CopyProperties(ContextDestination, ContextSource, Context);
	}
	for (const auto& [ContextSource, ContextDestination] : Context.GetObjects())
	{
		ContextDestination->PostDuplicate();
	}
	return Duplicate;
}

// 새 객체에 UUID와 배열 인덱스를 부여하고 전역 객체 배열에 등록한다.
UObject::UObject()
{
	if (FObjectInitializer* Initializer = FObjectInitializer::GetCurrent())
	{
		ClassPrivate = &Initializer->GetClass();
		Outer = Initializer->GetOuter();
		Name = Initializer->GetName();
		Flags = Initializer->GetFlags();
	}
	UUID = GenerateUUID();
	InternalIndex = static_cast<uint32>(GUObjectArray.size());
	GUObjectArray.push_back(this);
}

// 유효하지 않은 UUID로 예약된 0을 제외하고 새 객체 식별자를 순차 발급한다.
uint32 UObject::GenerateUUID()
{
	return NextUUID++;
}

// 제거할 객체를 배열의 마지막 객체와 교환해 전역 객체 배열에서 상수 시간에 제거한다.
UObject::~UObject()
{
	check(DefaultSubobjects.empty());
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

// 이 객체를 Template으로 사용해 같은 실제 클래스의 새 객체를 만든다.
UObject* UObject::Duplicate(UObject* NewOuter, FName NewName) const
{
	return GUObjectManager.DuplicateObject(*this, NewOuter ? NewOuter : Outer, std::move(NewName));
}

// 생성 중인 객체에 실제 런타임 클래스를 한 번만 연결한다.
void UObject::SetClass(UClass* InClass)
{
	panic(InClass);
	panic(ClassPrivate == nullptr);
	ClassPrivate = InClass;
}

// 이 객체가 생성자에서 만든 이름 있는 Default Subobject를 찾는다.
UObject* UObject::GetDefaultSubobject(const FName& Name) const
{
	for (UObject* Subobject : DefaultSubobjects)
	{
		if (Subobject && Subobject->GetObjectName() == Name)
		{
			return Subobject;
		}
	}
	return nullptr;
}

// 생성자에서 선언한 이름과 타입을 기준으로 Template의 대응 Subobject를 인스턴싱한다.
UObject* UObject::CreateDefaultSubobject(UClass& SubobjectClass, FName Name)
{
	panic(!Name.IsNone());
	panic(!GetDefaultSubobject(Name));

	FObjectInitializer& Initializer = FObjectInitializer::Get();
	check(&Initializer.GetClass() == GetClass());
	UObject* Template = Initializer.GetTemplate() ? Initializer.GetTemplate()->GetDefaultSubobject(Name) : nullptr;
	UClass* ActualClass = Template ? Template->GetClass() : &SubobjectClass;
	panic(ActualClass->IsChildOf(&SubobjectClass));
	UObject* Subobject = GUObjectManager.NewObject(
	    *ActualClass, this, Name, Template, EObjectFlags::DefaultSubobject, Initializer.GetInstancingContext());
	DefaultSubobjects.push_back(Subobject);
	return Subobject;
}

// 소유자가 직접 파괴하려는 Default Subobject를 생성 시 등록된 목록에서 제거한다.
void UObject::RemoveDefaultSubobject(UObject& Subobject)
{
	const auto Iterator = std::find(DefaultSubobjects.begin(), DefaultSubobjects.end(), &Subobject);
	check(Iterator != DefaultSubobjects.end());
	DefaultSubobjects.erase(Iterator);
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
