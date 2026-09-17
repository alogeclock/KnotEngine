#pragma once

#include "EngineAPI.h"

#include "Core/CoreTypes.h"
#include "Core/Name.h"

#include <type_traits>

class ENGINE_API FArchive
{
public:
	virtual ~FArchive() = default;

	virtual void Serialize(void* Data, int64 Size) = 0;
	virtual bool CanSerialize(int64 Size) const = 0;

	bool IsLoading() const { return bIsLoading; }
	bool IsSaving() const { return bIsSaving; }

	bool IsPersistent() const { return bIsPersistent; }
	bool IsSaveGame() const { return bIsSaveGame; }
	bool HasError() const { return bHasError; }
	void SetError() { bHasError = true; }

protected:
	bool bIsLoading = false;
	bool bIsSaving = false;

	bool bIsPersistent = false;
	bool bIsSaveGame = false;
	bool bHasError = false;
};

template <typename T>
FArchive& operator<<(FArchive& Ar, T& Value)
{
	static_assert(std::is_trivially_copyable_v<T>, "This type is not trivially copyable.");
	Ar.Serialize(&Value, sizeof(T));
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FString& String)
{
	static constexpr uint32 MaxLength = 65535;
	uint32 Length = 0;

	if (Ar.IsSaving())
	{
		if (String.size() > MaxLength)
		{
			Ar.SetError();
			return Ar;
		}
		Length = static_cast<uint32>(String.size());
	}

	Ar << Length;
	if (Ar.HasError() || Length > MaxLength || !Ar.CanSerialize(Length))
	{
		Ar.SetError();
		return Ar;
	}

	if (Ar.IsLoading())
	{
		String.resize(Length);
	}

	if (Length > 0)
	{
		Ar.Serialize(String.data(), static_cast<int64>(Length));
	}

	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FName& Name)
{
	FString String;

	if (Ar.IsSaving())
	{
		String = Name.ToString();
	}

	Ar << String;

	if (Ar.IsLoading() && !Ar.HasError())
	{
		Name = FName(String);
	}

	return Ar;
}
