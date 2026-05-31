#pragma once

#include <Windows.h>

#include "Runtime/Engine.h"

// 런처 진입점: 엔진 루프의 생성/실행/종료를 감쌉니다.
int Launch(HINSTANCE Instance, int ShowCmd);
