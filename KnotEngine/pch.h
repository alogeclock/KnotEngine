#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 5262)  // nlohmann/json lexer has intentional fallthrough paths.
#pragma warning(disable : 26819) // MSVC code analysis equivalent for unannotated fallthrough.
#endif

#include <nlohmann/json.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

using FJson = nlohmann::json;

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <SDKDDKVer.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>
#include <wrl/client.h>

#include "Core/CoreTypes.h"
#include "Core/Assert.h"
#include "Core/Debug.h"
#include "Core/Memory.h"
#include "Core/Singleton.h"

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Color.h"
#include "Core/Math/Utils.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Quat.h"
#include "Core/Math/Rotator.h"

#include "Core/Geometry/Transform.h"
#include "Core/Geometry/AABB.h"
#include "Core/Geometry/OBB.h"
#include "Core/Geometry/Ray.h"
#include "Core/Geometry/Plane.h"
#include "Core/Geometry/Frustum.h"
#include "Core/Geometry/Edge.h"
