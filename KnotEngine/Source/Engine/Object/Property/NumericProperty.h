#pragma once

#include "Object/Property.h"

#include <type_traits>

// 산술 타입의 초기화, 복사 및 직렬화 동작을 공통으로 제공한다.
template <typename T>
class TNumericProperty : public FProperty
{
public:
	TNumericProperty(FName InName, const UStruct* InOwner, uint32 InOffset, uint32 InArrayDimension = 1, EPropertyFlags InFlags = EPropertyFlags::None)
		: FProperty(std::move(InName), InOwner, InOffset, sizeof(T), InArrayDimension, InFlags)
	{
		static_assert(std::is_arithmetic_v<T>);
		static_assert(std::is_trivially_copyable_v<T>);
	}

protected:
	void InitializeElement(void* Value) const override { *static_cast<T*>(Value) = T{}; }
	void DestroyElement(void* Value) const override {}
	void CopyElement(void* Dst, const void* Src) const override { *static_cast<T*>(Dst) = *static_cast<const T*>(Src); }
	void SerializeElement(FArchive& Ar, void* Value) const override { Ar << *static_cast<T*>(Value); }
};

class FIntProperty final : public TNumericProperty<int32>
{
public:
	using TNumericProperty::TNumericProperty;
};

class FBoolProperty final : public TNumericProperty<bool>
{
public:
	using TNumericProperty::TNumericProperty;
};

class FFloatProperty final : public TNumericProperty<float>
{
public:
	using TNumericProperty::TNumericProperty;
};

class FDoubleProperty final : public TNumericProperty<double>
{
public:
	using TNumericProperty::TNumericProperty;
};
