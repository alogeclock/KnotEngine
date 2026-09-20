#pragma once

#include "EngineAPI.h"

#include "Core/Geometry/Transform.h"
#include "Core/Math/Color.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector4.h"
#include "Render/RHI/RenderTypes.h"
#include "Render/Resource/Mesh/GeometryMesh.h"

class FDebugDrawPass;
class IRenderDevice;

// 한 프레임 동안 제출된 선과 기본 도형을 배치하고, 반복 도형의 단위 Mesh GPU 자원을 소유한다.
class ENGINE_API FDebugDraw final
{
public:
	FDebugDraw() = default;
	~FDebugDraw();

	FDebugDraw(const FDebugDraw&) = delete;
	FDebugDraw& operator=(const FDebugDraw&) = delete;
	FDebugDraw(FDebugDraw&&) = delete;
	FDebugDraw& operator=(FDebugDraw&&) = delete;

	void Create();
	void Prepare(IRenderDevice& RenderDevice);
	void Release();
	void Reset();
	bool IsEmpty() const;

	void DrawLine(const FVector& Start, const FVector& End, const FColor& Color = FColor::White());
	void DrawCube(const FVector& Center, const FVector& Extent, const FQuat& Rotation = FQuat::Identity, const FColor& Color = FColor::White());
	void DrawSphere(const FVector& Center, float Radius, const FColor& Color = FColor::White());
	void DrawHemiSphere(const FVector& Center, float Radius, const FQuat& Rotation = FQuat::Identity, const FColor& Color = FColor::White());
	void DrawCylinder(const FVector& Center, float Radius, float HalfHeight, const FQuat& Rotation = FQuat::Identity, const FColor& Color = FColor::White());
	void DrawCapsule(const FVector& Center, float Radius, float HalfHeight, const FQuat& Rotation = FQuat::Identity, const FColor& Color = FColor::White());
	void DrawArrow(const FVector& Start, const FVector& End, const FColor& Color = FColor::White());

private:
	friend class FDebugDrawPass;

	enum class EShapeType : uint8
	{
		Cube,
		Sphere,
		Hemisphere,
		Cylinder,
		Capsule,
		Arrow,
		Count,
	};

	struct alignas(16) FInstance
	{
		FMatrix Model;
		FVector4 Color;
		FVector4 ShapeParameters;
	};
	static_assert(sizeof(FInstance) == 96);

	static constexpr uint32 ShapeTypeCount = static_cast<uint32>(EShapeType::Count);

	static void AddLineSegment(TArray<FGeometryVertex>& Vertices, const FVector& Start, const FVector& End, uint32 Color);
	static void AddMeshLine(TArray<FGeometryVertex>& Vertices, TArray<uint32>& Indices, const FVector& Start, const FVector& End);
	static void AddCircle(TArray<FGeometryVertex>& Vertices, TArray<uint32>& Indices, EAxis Axis, float AxisOffset, float Radius, uint32 Segments);

	static void BuildCubeMesh(FGeometryMesh& Mesh);
	static void BuildSphereMesh(FGeometryMesh& Mesh);
	static void BuildHemisphereMesh(FGeometryMesh& Mesh);
	static void BuildCylinderMesh(FGeometryMesh& Mesh);
	static void BuildCapsuleMesh(FGeometryMesh& Mesh);
	static void BuildArrowMesh(FGeometryMesh& Mesh);

	static FMatrix MakeArrowTransform(const FVector& Start, const FVector& End);

	void AddInstance(EShapeType ShapeType, const FMatrix& Model, const FColor& Color, const FVector4& ShapeParameters);
	const FGeometryMesh& GetShapeMesh(EShapeType ShapeType) const;
	void PrepareLineBuffer(IRenderDevice& RenderDevice);

	FGeometryMesh CubeMesh;
	FGeometryMesh SphereMesh;
	FGeometryMesh HemisphereMesh;
	FGeometryMesh CylinderMesh;
	FGeometryMesh CapsuleMesh;
	FGeometryMesh ArrowMesh;

	TArray<FGeometryVertex> LineVertices;
	TArray<FInstance> Instances[ShapeTypeCount];
	FBufferHandle LineBuffer;
	uint32 LineBufferCapacity = 0;
	IRenderDevice* ResourceOwner = nullptr;
};
