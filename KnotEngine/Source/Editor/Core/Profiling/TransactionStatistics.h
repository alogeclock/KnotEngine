#pragma once

#include "Core/CoreTypes.h"

// 실제로 적용된 Undo 또는 Redo의 마지막 레코드 수와 누적 실행 시간을 보관한다.
struct FTransactionOperationStatistics
{
	uint64 OperationCount = 0;
	uint64 RecordCount = 0;
	double TotalTimeMs = 0.0;
	double AccumulatedTimeMs = 0.0;
	double MaxTimeMs = 0.0;
	FString Description;
};

// Undo와 Redo를 각각 집계하며 History가 초기화되어도 측정 결과는 유지한다.
struct FTransactionStatistics
{
	FTransactionOperationStatistics Undo;
	FTransactionOperationStatistics Redo;
};
