#pragma once

class UClass;
class UScriptStruct;
class FReflectionRegistry;

template <typename T>
struct TReflectionAccess;

// C++ 컴파일 시 아무 기능도 하지 않는 Python Parser 전용 마커
// 설명은 마커 바로 위의 /// 주석만 사용한다.
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)

// 클래스별 진입점과 생성 코드의 접근 권한을 선언한다. 정의는 .gen.cpp에 생성한다.
#define GENERATED_CLASS(ClassName, ParentClass) \
private:                                        \
	static UClass* StaticClassPrivate;          \
	friend class FReflectionRegistry;           \
	friend struct TReflectionAccess<ClassName>; \
                                                \
public:                                         \
	using ThisClass = ClassName;                \
	using Super = ParentClass;                  \
	static UClass* StaticClass();

#define GENERATED_STRUCT(StructName)             \
private:                                         \
	static UScriptStruct* StaticStructPrivate;   \
	friend class FReflectionRegistry;            \
	friend struct TReflectionAccess<StructName>; \
                                                 \
public:                                          \
	static UScriptStruct* StaticStruct();
