#pragma once

#include "Object/Object.h"

// UField 스켈레톤 구현, 추후 FProperty 및 Reflection System 구현과 함께 재구현
class UField : public UObject
{
public:
    UField(const char* InName, const char* InDisplayName = nullptr, const char* InCategory = nullptr)
        : Name(InName), DisplayName(InDisplayName), Category(InCategory)
    {
    }

    const char* GetName() const { return Name; }
    const char* GetDisplayName() const { return DisplayName ? DisplayName : Name; }
    const char* GetCategory() const { return Category; }

protected:
    const char* Name = nullptr;
    const char* DisplayName = nullptr;
    const char* Category = nullptr;
};

// UStruct 생략, 추후 FProperty 및 Reflection System 구현과 함께 재구현
class UClass : public UField
{
    UClass* GetSuperClass() const { return SuperClass; }
    size_t GetClassSize() const { return ClassSize; }
    size_t GetMinAlignment() const { return MinAlignment; }

protected:
    UClass* SuperClass = nullptr;
    size_t ClassSize = 0;
    size_t MinAlignment = 0;
};
