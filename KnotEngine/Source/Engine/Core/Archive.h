#pragma once

#include "Core/CoreTypes.h"
#include "Core/Name.h"

#include <limits>
#include <type_traits>

class FArchive
{
public:
    virtual ~FArchive() = default;

    virtual void Serialize(void* Data, int64 Size) = 0;

    bool IsLoading() const { return bIsLoading; }
    bool IsSaving() const { return bIsSaving; }
    bool IsPersistent() const { return bIsPersistent; }
    bool IsSaveGame() const { return bIsSaveGame; }
    bool IsObjectReferenceCollector() const { return bIsObjectReferenceCollector; }

    template <typename T>
    FArchive& operator<<(T& Value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "This type is not trivially copyable.");
        this->Serialize(&Value, sizeof(T));
        return *this;
    }

protected:
    bool bIsLoading = false;
    bool bIsSaving = false;
    bool bIsPersistent = false;
    bool bIsSaveGame = false;
    bool bIsObjectReferenceCollector = false;
};

inline FArchive& operator<<(FArchive& Ar, FString& String)
{
    uint32 Length = 0;

    if (Ar.IsSaving())
    {
        check(String.size() <= static_cast<size_t>(std::numeric_limits<uint32>::max()));
        Length = static_cast<uint32>(String.size());
    }

    Ar << Length;

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

    if (Ar.IsLoading())
    {
        Name = FName(String);
    }

    return Ar;
}
