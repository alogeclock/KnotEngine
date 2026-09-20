#pragma once

#include "EngineAPI.h"

#include <filesystem>

class UWorld;

// World의 Level, Node와 Component 객체 그래프를 사람이 읽을 수 있는 YAML .kmap으로 저장하고 복원한다.
// TODO: 추후 UObject를 파일 단위로 묶는 범용 직렬화 구조가 필요할 경우 UPackage 구현을 고려한다.
class ENGINE_API FMapSerializer final
{
public:
	bool Save(UWorld& World, const std::filesystem::path& FilePath) const;
	bool Load(UWorld& World, const std::filesystem::path& FilePath) const;
};
