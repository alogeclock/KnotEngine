#include "Core/Archive/StructuredArchive.h"

FStructuredArchive::FStructuredArchive(FStructuredArchiveFormatter& InFormatter, FStructuredArchiveObjectResolver* InObjectResolver)
	: Formatter(InFormatter), ObjectResolver(InObjectResolver)
{
}

// Archive의 유일한 Root Slot을 연다.
FStructuredArchive::FSlot FStructuredArchive::Open()
{
	check(!bOpened);
	bOpened = true;
	return FSlot(*this, Formatter.GetRootElement());
}

FStructuredArchiveSlot::FStructuredArchiveSlot(FStructuredArchive& InArchive, FStructuredArchiveElement InElement)
	: Archive(&InArchive), Element(InElement)
{
}

// 현재 Slot을 이름이 붙은 Field의 Record로 연다.
FStructuredArchiveRecord FStructuredArchiveSlot::EnterRecord() const
{
	SerializeResult(Archive->Formatter.EnterRecord(Element));
	return FStructuredArchiveRecord(*Archive, Element);
}

// 현재 Slot을 지정된 원소 개수의 Array로 연다.
FStructuredArchiveArray FStructuredArchiveSlot::EnterArray(uint32& Count) const
{
	SerializeResult(Archive->Formatter.EnterArray(Element, Count));
	return FStructuredArchiveArray(*Archive, Element, Count);
}

bool FStructuredArchiveSlot::IsLoading() const
{
	return Archive->IsLoading();
}

bool FStructuredArchiveSlot::IsSaving() const
{
	return Archive->IsSaving();
}

bool FStructuredArchiveSlot::IsNull() const
{
	return Archive->Formatter.IsNull(Element);
}

// 현재 Slot을 명시적인 null 값으로 기록한다.
void FStructuredArchiveSlot::SetNull() const
{
	SerializeResult(Archive->Formatter.SetNull(Element));
}

FStructuredArchive& FStructuredArchiveSlot::GetArchive() const
{
	return *Archive;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(bool& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(int32& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(uint32& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(uint64& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(float& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(double& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

FStructuredArchiveSlot& FStructuredArchiveSlot::operator<<(FString& Value)
{
	SerializeResult(Archive->Formatter.Serialize(Element, Value));
	return *this;
}

void FStructuredArchiveSlot::SerializeResult(bool bSuccess) const
{
	if (!bSuccess)
	{
		Archive->SetError();
	}
}

FStructuredArchiveRecord::FStructuredArchiveRecord(FStructuredArchive& InArchive, FStructuredArchiveElement InElement)
	: Archive(&InArchive), Element(InElement)
{
}

// 필수 Field에 진입하고 없거나 형식이 잘못된 경우 Archive 오류를 기록한다.
FStructuredArchiveSlot FStructuredArchiveRecord::EnterField(std::string_view Name) const
{
	FStructuredArchiveElement FieldElement = 0;
	if (!Archive->Formatter.EnterField(Element, Name, Archive->IsSaving(), FieldElement))
	{
		Archive->SetError();
	}
	return FStructuredArchiveSlot(*Archive, FieldElement);
}

// 저장 중에는 Field를 만들고, 불러오는 중에는 존재하는 Field에만 진입한다.
std::optional<FStructuredArchiveSlot> FStructuredArchiveRecord::TryEnterField(std::string_view Name) const
{
	FStructuredArchiveElement FieldElement = 0;
	if (!Archive->Formatter.EnterField(Element, Name, Archive->IsSaving(), FieldElement))
	{
		return std::nullopt;
	}
	return FStructuredArchiveSlot(*Archive, FieldElement);
}

bool FStructuredArchiveRecord::IsLoading() const
{
	return Archive->IsLoading();
}

bool FStructuredArchiveRecord::IsSaving() const
{
	return Archive->IsSaving();
}

FStructuredArchiveArray::FStructuredArchiveArray(FStructuredArchive& InArchive, FStructuredArchiveElement InElement, uint32 InCount)
	: Archive(&InArchive), Element(InElement), Count(InCount)
{
}

// Array의 다음 원소 Slot에 순서대로 진입한다.
FStructuredArchiveSlot FStructuredArchiveArray::EnterElement()
{
	if (NextIndex >= Count)
	{
		Archive->SetError();
		return FStructuredArchiveSlot(*Archive, 0);
	}

	FStructuredArchiveElement ChildElement = 0;
	if (!Archive->Formatter.EnterArrayElement(Element, NextIndex++, ChildElement))
	{
		Archive->SetError();
	}
	return FStructuredArchiveSlot(*Archive, ChildElement);
}

bool FStructuredArchiveArray::IsLoading() const
{
	return Archive->IsLoading();
}

bool FStructuredArchiveArray::IsSaving() const
{
	return Archive->IsSaving();
}
