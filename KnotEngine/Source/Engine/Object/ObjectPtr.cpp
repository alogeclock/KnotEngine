#include "ObjectPtr.h"

#include "Object/ReferenceCollector.h"

// 포인터 저장 형식에서 UObject를 꺼내 참조 수집기에 전달한다.
void IObjectPtrOps::VisitReference(void* Value, FReferenceCollector& Collector) const
{
	Collector.AddReferencedObject(GetObject(Value));
}
