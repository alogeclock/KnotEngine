#include "ReferenceCollector.h"

#include "Object/Class.h"
#include "Object/Property.h"

// 이전 결과를 지우고 지정한 루트 객체에서 도달 가능한 모든 강한 참조를 수집한다.
void FReferenceCollector::CollectReferences(UObject* RootObject)
{
	ReachableObjects.clear();
	VisitObject(RootObject);
}

// 프로퍼티나 객체의 수동 참조 목록에서 발견한 객체를 방문한다.
void FReferenceCollector::AddReferencedObject(UObject* Object)
{
	VisitObject(Object);
}

// 처음 발견한 객체를 기록하고 수동 참조와 리플렉션 프로퍼티를 재귀적으로 방문한다.
void FReferenceCollector::VisitObject(UObject* Object)
{
	if (!Object || !ReachableObjects.emplace(Object).second)
	{
		return;
	}

	Object->AddReferencedObjects(*this);

	const UClass* Class = Object->GetClass();
	if (!Class)
	{
		return;
	}

	TArray<const FProperty*> Properties;
	Class->GetAllProperties(Properties);
	for (const FProperty* Property : Properties)
	{
		Property->VisitReferencesInContainer(Object, *this);
	}
}
