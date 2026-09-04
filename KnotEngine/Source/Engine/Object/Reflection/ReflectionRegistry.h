#pragma once

#include "Core/CoreTypes.h"
#include "Core/Name.h"

#include <memory>

class UClass;
class UEnum;
class UField;
class UScriptStruct;

// 생성된 타입 스키마의 전역 소유권과 이름 기반 조회를 담당한다.
class FReflectionRegistry
{
public:
	FReflectionRegistry();
	~FReflectionRegistry();

	FReflectionRegistry(const FReflectionRegistry&) = delete;
	FReflectionRegistry& operator=(const FReflectionRegistry&) = delete;

	void Startup();
	void Shutdown();

	UClass* RegisterClass(std::unique_ptr<UClass> Class);
	UScriptStruct* RegisterScriptStruct(std::unique_ptr<UScriptStruct> Struct);
	UEnum* RegisterEnum(std::unique_ptr<UEnum> Enum);

	UField* FindField(const FName& Name) const;
	UClass* FindClass(const FName& Name) const;
	UScriptStruct* FindScriptStruct(const FName& Name) const;
	UEnum* FindEnum(const FName& Name) const;

private:
	struct FNameHash
	{
		SIZE_T operator()(const FName& Name) const { return GetTypeHash(Name); }
	};

	void RegisterStaticClass();
	void ResetStaticClass();

	UField* RegisterField(std::unique_ptr<UField> Field);

	TArray<std::unique_ptr<UField>> Fields;
	TMap<FName, UField*, FNameHash> FieldsByName;
};

extern FReflectionRegistry* GReflectionRegistry;

// UENUM 타입의 특수화는 생성된 cpp에서 정의한다.
template <typename T>
UEnum* StaticEnum();
