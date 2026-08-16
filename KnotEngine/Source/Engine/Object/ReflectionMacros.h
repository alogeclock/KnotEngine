#pragma once

class UClass;
class UScriptStruct;

// C++ 컴파일 시 아무 기능도 하지 않는 Python Parser 전용 마커
#define UCLASS(...)
#define UPROPERTY(...)
#define UENUM(...)
#define UMETA(...)
#define USTRUCT(...)
#define UFUNCTION(...)
#define UDELEGATE(...)
#define UINTERFACE(...)

// 런타임 UClass 진입점 표시
#define GENERATED_CLASS(ClassName, ParentClass) \
public:                                         \
	using ThisClass = ClassName;                \
	using Super = ParentClass;                  \
	static UClass* StaticClass();               \
	virtual UClass* GetClass() const override   \
	{                                           \
		return StaticClass();                   \
	}                                           \
	friend struct Z_Construct_UClass_##ClassName;

// 런타임 UScriptSturct 진입점 표시
#define GENERATED_STRUCT(StructName)            \
public:                                         \
	static const UScriptStruct* StaticStruct(); \
	friend struct Z_Construct_UScriptStruct_##StructName;
