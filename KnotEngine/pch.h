#pragma once

#include "CoreTypes.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <nlohmann/json.hpp>
using FJson = nlohmann::json;

#define WIN32_LEAN_AND_MEAN
#include <SDKDDKVer.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <iostream>
#include <wrl/client.h>

#include "Math/Vector2.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Color.h"
#include "Math/Utils.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"

#include "Geometry/Transform.h"
#include "Geometry/AABB.h"
#include "Geometry/OBB.h"
#include "Geometry/Ray.h"
#include "Geometry/Plane.h"
#include "Geometry/Frustum.h"
#include "Geometry/Edge.h"
