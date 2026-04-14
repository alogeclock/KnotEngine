#pragma once

// Target Windows 버전 설정 (선택사항이나 권장)
#include <SDKDDKVer.h>

// Windows 헤더에서 불필요한 매크로(min, max 등) 제외
#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX
#include <Windows.h>

// C++ STL
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// DirectX 11 & Math
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// WRL (ComPtr용)
#include <wrl/client.h>
