#pragma once

#include "Asset/Asset/AssetId.h"

// 엔진 코드가 직접 참조하는 내장 Asset의 고정 ID다.
class FEngineAssetIds final
{
public:
	inline static constexpr FAssetId DefaultWhiteMaterial = { 0x40E1E67F265FAEA4, 0xC76A12B702AAF3B5 };
	inline static constexpr FAssetId Capsule = { 0x4EC40B4D93A527A1, 0xD6F7586DC6FAD590 };
	inline static constexpr FAssetId Cube = { 0x4A100E220DB88E76, 0xDC71D79FC7FAC19D };
	inline static constexpr FAssetId Cylinder = { 0x48A5F6AB918D9A76, 0x510F18FB2807038E };
	inline static constexpr FAssetId Quad = { 0x42C4C43765C119B5, 0x345AF10C0E6BDE99 };
	inline static constexpr FAssetId Sphere = { 0x426B3AD84CA89D44, 0x6C7A58F1DDC5EDAC };
};
