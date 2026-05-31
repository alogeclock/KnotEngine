#pragma once

// CRTP (Curiously Recurring Template Pattern) 싱글턴 베이스 클래스
// TSingleton을 상속받으면서 자기 자신을 Template 인자로 넘겨 싱글턴 구현
template <typename T>
class TSingleton
{
public:
    static T& Get()
    {
        static T Instance; // C++11 이후 지역 정적 변수 초기화는 thread-safe
        return Instance;
    }

    // delete 키워드를 통해 인스턴스 유일 보장 (복사 및 이동 제한)
    TSingleton(const TSingleton&) = delete;
    TSingleton& operator=(const TSingleton&) = delete;
    TSingleton(TSingleton&&) = delete;
    TSingleton& operator=(TSingleton&&) = delete;

protected:
    TSingleton() = default;
    ~TSingleton() = default;
};