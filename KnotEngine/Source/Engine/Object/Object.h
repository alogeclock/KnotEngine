#pragma once

class UObject
{
public:
	UObject();
	virtual ~UObject();

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; } 

private:
	uint32 UUID;
	uint32 InternalIndex;
};

extern TArray<UObject*> GUObjectArray;

class FUObjectManager
{
public:
	template<typename T> 
	T* Create()
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must derive from UObject");
		T* Object = new T();
		return Object;
	}

	void Destroy(UObject* Object)
	{
		if (!Object) 
		{
			return;
		}

		delete Object;
	}

	UObject* FindByUUID(uint32 UUID)
	{
		for (auto* Obj : GUObjectArray)
		{
			if (Obj && Obj->GetUUID() == UUID)
			{
				return Obj;
			}
		}

		return nullptr;
	}

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size())
		{
			return nullptr;
		}
		
		return GUObjectArray[Index];
	}
};

extern FUObjectManager GUObjectManager;
