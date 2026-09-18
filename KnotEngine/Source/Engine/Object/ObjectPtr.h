#pragma once

#include "EngineAPI.h"

#include "Asset/Asset/AssetId.h"
#include "Core/CoreTypes.h"

#include <cstddef>
#include <new>
#include <type_traits>

class FReferenceCollector;
class UObject;

template <typename T>
class TObjectPtr
{
public:
	TObjectPtr() = default;
	TObjectPtr(std::nullptr_t) {}
	TObjectPtr(T* InObject) : Object(InObject)
	{
		static_assert(std::is_base_of_v<UObject, T>, "TObjectPtr can only point to UObject-derived types.");
	}

	T* Get() const { return Object; }
	void Set(T* InObject) { Object = InObject; }
	bool IsNull() const { return Object == nullptr; }
	T* operator->() const { return Get(); }
	T& operator*() const { return *Get(); }
	explicit operator bool() const { return Object != nullptr; }

	TObjectPtr& operator=(T* InObject)
	{
		static_assert(std::is_base_of_v<UObject, T>, "TObjectPtr can only point to UObject-derived types.");
		Object = InObject;
		return *this;
	}

	TObjectPtr& operator=(std::nullptr_t)
	{
		Object = nullptr;
		return *this;
	}

	operator T*() const { return Get(); }

private:
	T* Object = nullptr;
};

// FObjectProperty가 실제 저장 형식(raw pointer 또는 TObjectPtr)을 알지 않고 객체 참조를 다루기 위한 타입 소거 인터페이스.
class ENGINE_API IObjectPtrOps
{
public:
	virtual ~IObjectPtrOps() = default;

	virtual uint32 GetValueSize() const = 0;
	virtual void InitializeValue(void* Value) const = 0;
	virtual void DestroyValue(void* Value) const = 0;
	virtual void CopyValue(void* Dst, const void* Src) const = 0;
	virtual UObject* GetObject(const void* Value) const = 0;
	virtual void SetObject(void* Value, UObject* Object) const = 0;
	virtual void VisitReference(void* Value, FReferenceCollector& Collector) const;
};

template <typename T>
class TRawObjectPtrOps final : public IObjectPtrOps
{
public:
	uint32 GetValueSize() const override { return sizeof(T*); }

	void InitializeValue(void* Value) const override
	{
		new (Value) T*(nullptr);
	}

	void DestroyValue(void*) const override {}

	void CopyValue(void* Dst, const void* Src) const override
	{
		*static_cast<T**>(Dst) = *static_cast<T* const*>(Src);
	}

	UObject* GetObject(const void* Value) const override
	{
		return *static_cast<T* const*>(Value);
	}

	void SetObject(void* Value, UObject* Object) const override
	{
		*static_cast<T**>(Value) = static_cast<T*>(Object);
	}
};

template <typename T>
class TObjectPtrOps final : public IObjectPtrOps
{
public:
	uint32 GetValueSize() const override { return sizeof(TObjectPtr<T>); }

	void InitializeValue(void* Value) const override
	{
		new (Value) TObjectPtr<T>();
	}

	void DestroyValue(void* Value) const override
	{
		using ObjectPtrType = TObjectPtr<T>;
		static_cast<ObjectPtrType*>(Value)->~ObjectPtrType();
	}

	void CopyValue(void* Dst, const void* Src) const override
	{
		*static_cast<TObjectPtr<T>*>(Dst) = *static_cast<const TObjectPtr<T>*>(Src);
	}

	UObject* GetObject(const void* Value) const override
	{
		return static_cast<const TObjectPtr<T>*>(Value)->Get();
	}

	void SetObject(void* Value, UObject* Object) const override
	{
		static_cast<TObjectPtr<T>*>(Value)->Set(static_cast<T*>(Object));
	}
};

template <typename T>
const IObjectPtrOps* GetRawObjectPtrOps()
{
	static_assert(std::is_base_of_v<UObject, T>, "Raw reflected object pointers must point to UObject-derived types.");
	static const TRawObjectPtrOps<T> Ops;
	return &Ops;
}

template <typename T>
const IObjectPtrOps* GetTObjectPtrOps()
{
	static_assert(std::is_base_of_v<UObject, T>, "TObjectPtr can only point to UObject-derived types.");
	static const TObjectPtrOps<T> Ops;
	return &Ops;
}

template <typename T>
class TSoftObjectPtr
{
public:
	TSoftObjectPtr() = default;
	TSoftObjectPtr(std::nullptr_t) {}
	explicit TSoftObjectPtr(const FAssetId& InAssetId) : AssetId(InAssetId)
	{
		static_assert(std::is_base_of_v<UObject, T>, "TSoftObjectPtr can only point to UObject-derived types.");
	}

	const FAssetId& GetAssetId() const { return AssetId; }
	void SetAssetId(const FAssetId& InAssetId)
	{
		AssetId = InAssetId;
		CachedObject = nullptr;
	}

	T* Get() const { return CachedObject; }
	void SetResolvedObject(T* InObject) const
	{
		check(!InObject || (AssetId.IsValid() && InObject->GetAssetId() == AssetId));
		CachedObject = InObject;
	}
	void ResetResolvedObject() const { CachedObject = nullptr; }
	void Reset()
	{
		AssetId = {};
		CachedObject = nullptr;
	}

	bool IsNull() const { return !AssetId.IsValid(); }
	bool IsPending() const { return AssetId.IsValid() && CachedObject == nullptr; }
	T* operator->() const { return Get(); }
	explicit operator bool() const { return CachedObject != nullptr; }

	TSoftObjectPtr& operator=(std::nullptr_t)
	{
		Reset();
		return *this;
	}

private:
	FAssetId AssetId; // AssetId만 논리적인 소프트 참조. 이 포인터는 해석 결과를 재사용하는 비소유 캐시이며 직렬화와 강한 참조 수집에서 제외된다.
	mutable T* CachedObject = nullptr;
};

// FSoftObjectProperty가 TSoftObjectPtr의 템플릿 인자를 알지 않고 영속 Asset ID를 다루기 위한 타입 소거 인터페이스.
class ENGINE_API ISoftObjectPtrOps
{
public:
	virtual ~ISoftObjectPtrOps() = default;

	virtual uint32 GetValueSize() const = 0;
	virtual void InitializeValue(void* Value) const = 0;
	virtual void DestroyValue(void* Value) const = 0;
	virtual void CopyValue(void* Dst, const void* Src) const = 0;
	virtual const FAssetId& GetAssetId(const void* Value) const = 0;
	virtual void SetAssetId(void* Value, const FAssetId& AssetId) const = 0;
};

template <typename T>
class TSoftObjectPtrOps final : public ISoftObjectPtrOps
{
public:
	uint32 GetValueSize() const override { return sizeof(TSoftObjectPtr<T>); }

	void InitializeValue(void* Value) const override
	{
		new (Value) TSoftObjectPtr<T>();
	}

	void DestroyValue(void* Value) const override
	{
		using SoftObjectPtrType = TSoftObjectPtr<T>;
		static_cast<SoftObjectPtrType*>(Value)->~SoftObjectPtrType();
	}

	void CopyValue(void* Dst, const void* Src) const override
	{
		*static_cast<TSoftObjectPtr<T>*>(Dst) = *static_cast<const TSoftObjectPtr<T>*>(Src);
	}

	const FAssetId& GetAssetId(const void* Value) const override
	{
		return static_cast<const TSoftObjectPtr<T>*>(Value)->GetAssetId();
	}

	void SetAssetId(void* Value, const FAssetId& AssetId) const override
	{
		static_cast<TSoftObjectPtr<T>*>(Value)->SetAssetId(AssetId);
	}
};

template <typename T>
const ISoftObjectPtrOps* GetTSoftObjectPtrOps()
{
	static_assert(std::is_base_of_v<UObject, T>, "TSoftObjectPtr can only point to UObject-derived types.");
	static const TSoftObjectPtrOps<T> Ops;
	return &Ops;
}
