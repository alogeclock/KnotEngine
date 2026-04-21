#pragma once

//=============================================================================
// Platform Types & Containers
// Basic integer and platform-dependent types.
// Example: TArray, TMap, int32, uint32, TCHAR.
//=============================================================================
#include "CoreTypes.h"

//=============================================================================
// Target Windows Version Settings & DirectX Header
//=============================================================================
#define WIN32_LEAN_AND_MEAN 
#define NOMINMAX
#include <SDKDDKVer.h>
#include <Windows.h>
#include <iostream>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>

//=============================================================================
// Math
// Core math types and utilities.
// Vector, matrix, rotation, transform, color, etc.
//=============================================================================
#include "Math/Vector2.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#include "Math/Utils.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"

//=============================================================================
// Geometry
// Geometry Primitives & Utility
// AABB, Ray, Segment, Triangle, 교차 판정, Bounds 보조 함수 등
//=============================================================================
#include "Geometry/Transform.h"
#include "Geometry/AABB.h"
#include "Geometry/OBB.h"
#include "Geometry/Ray.h"
#include "Geometry/Plane.h"
#include "Geometry/Frustum.h"
#include "Geometry/Edge.h"
