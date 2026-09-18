#include "Render/DebugDraw/DebugDraw.h"

#include "Core/Assert.h"
#include "Core/Math/Math.h"
#include "Render/RHI/RenderDevice.h"

#include <algorithm>
#include <cmath>
#include <limits>

FDebugDraw::~FDebugDraw()
{
	Release();
}

// Renderer 수명 동안 공유할 기본 Wire Geometry의 CPU 데이터를 생성한다.
void FDebugDraw::Create()
{
	Release();
	BuildCubeMesh(CubeMesh);
	BuildSphereMesh(SphereMesh);
	BuildHemisphereMesh(HemisphereMesh);
	BuildCylinderMesh(CylinderMesh);
	BuildCapsuleMesh(CapsuleMesh);
	BuildArrowMesh(ArrowMesh);
}

// 기본 도형 Mesh와 이번 프레임의 동적 Line Buffer를 Draw Pass 구성 전에 준비한다.
void FDebugDraw::Prepare(IRenderDevice& RenderDevice)
{
	ResourceOwner = &RenderDevice;
	panicf(CubeMesh.InitResources(RenderDevice), "Debug Cube Mesh의 GPU Buffer 생성에 실패했다.");
	panicf(SphereMesh.InitResources(RenderDevice), "Debug Sphere Mesh의 GPU Buffer 생성에 실패했다.");
	panicf(HemisphereMesh.InitResources(RenderDevice), "Debug Hemisphere Mesh의 GPU Buffer 생성에 실패했다.");
	panicf(CylinderMesh.InitResources(RenderDevice), "Debug Cylinder Mesh의 GPU Buffer 생성에 실패했다.");
	panicf(CapsuleMesh.InitResources(RenderDevice), "Debug Capsule Mesh의 GPU Buffer 생성에 실패했다.");
	panicf(ArrowMesh.InitResources(RenderDevice), "Debug Arrow Mesh의 GPU Buffer 생성에 실패했다.");
	PrepareLineBuffer(RenderDevice);
}

// FDebugDraw가 소유한 동적 Buffer와 기본 도형 GPU 자원을 해제한다.
void FDebugDraw::Release()
{
	if (ResourceOwner)
	{
		if (LineBuffer.IsValid())
		{
			ResourceOwner->DestroyBuffer(LineBuffer);
		}
	}
	LineBufferCapacity = 0;
	ResourceOwner = nullptr;

	CubeMesh.Release();
	SphereMesh.Release();
	HemisphereMesh.Release();
	CylinderMesh.Release();
	CapsuleMesh.Release();
	ArrowMesh.Release();
	Reset();
}

// 한 프레임 동안 수집한 CPU Debug Draw 명령을 비운다.
void FDebugDraw::Reset()
{
	LineVertices.clear();
	for (TArray<FInstance>& ShapeInstances : Instances)
	{
		ShapeInstances.clear();
	}
}

bool FDebugDraw::IsEmpty() const
{
	if (!LineVertices.empty())
	{
		return false;
	}
	for (const TArray<FInstance>& ShapeInstances : Instances)
	{
		if (!ShapeInstances.empty())
		{
			return false;
		}
	}
	return true;
}

void FDebugDraw::DrawLine(const FVector& Start, const FVector& End, const FColor& Color)
{
	if (!Start.Equals(End))
	{
		AddLineSegment(LineVertices, Start, End, Color.ToPackedABGR());
	}
}

void FDebugDraw::DrawCube(
	const FVector& Center,
	const FVector& Extent,
	const FQuat& Rotation,
	const FColor& Color)
{
	if (Extent.X > 0.0f && Extent.Y > 0.0f && Extent.Z > 0.0f)
	{
		AddInstance(EShapeType::Cube, FTransform(Rotation, Center, Extent).ToMatrix(), Color, FVector4());
	}
}

void FDebugDraw::DrawSphere(const FVector& Center, float Radius, const FColor& Color)
{
	if (Radius > 0.0f)
	{
		AddInstance(EShapeType::Sphere, FTransform(FQuat::Identity, Center, FVector(Radius, Radius, Radius)).ToMatrix(), Color, FVector4());
	}
}

void FDebugDraw::DrawHemiSphere(
	const FVector& Center,
	float Radius,
	const FQuat& Rotation,
	const FColor& Color)
{
	if (Radius > 0.0f)
	{
		AddInstance(EShapeType::Hemisphere, FTransform(Rotation, Center, FVector(Radius, Radius, Radius)).ToMatrix(), Color, FVector4());
	}
}

void FDebugDraw::DrawCylinder(
	const FVector& Center,
	float Radius,
	float HalfHeight,
	const FQuat& Rotation,
	const FColor& Color)
{
	if (Radius > 0.0f && HalfHeight > 0.0f)
	{
		AddInstance(EShapeType::Cylinder, FTransform(Rotation, Center, FVector(Radius, Radius, HalfHeight)).ToMatrix(), Color, FVector4());
	}
}

void FDebugDraw::DrawCapsule(
	const FVector& Center,
	float Radius,
	float HalfHeight,
	const FQuat& Rotation,
	const FColor& Color)
{
	if (Radius > 0.0f && HalfHeight >= 0.0f)
	{
		const FMatrix Model = FTransform(Rotation, Center).ToMatrix();
		const float CylinderHalfHeight = std::max(HalfHeight - Radius, 0.0f);
		AddInstance(EShapeType::Capsule, Model, Color, FVector4(Radius, CylinderHalfHeight, 1.0f, 0.0f));
	}
}

void FDebugDraw::DrawArrow(const FVector& Start, const FVector& End, const FColor& Color)
{
	if (!Start.Equals(End))
	{
		AddInstance(EShapeType::Arrow, MakeArrowTransform(Start, End), Color, FVector4());
	}
}

void FDebugDraw::AddLineSegment(TArray<FGeometryVertex>& Vertices, const FVector& Start, const FVector& End, uint32 Color)
{
	Vertices.push_back({ Start, Color });
	Vertices.push_back({ End, Color });
}

void FDebugDraw::AddMeshLine(TArray<FGeometryVertex>& Vertices, TArray<uint32>& Indices, const FVector& Start, const FVector& End)
{
	static constexpr uint32 White = 0xFFFFFFFF;
	check(Vertices.size() <= (std::numeric_limits<uint32>::max)() - 2);
	const uint32 FirstVertex = static_cast<uint32>(Vertices.size());
	Vertices.push_back({ Start, White });
	Vertices.push_back({ End, White });
	Indices.push_back(FirstVertex);
	Indices.push_back(FirstVertex + 1);
}

void FDebugDraw::AddCircle(
	TArray<FGeometryVertex>& Vertices,
	TArray<uint32>& Indices,
	EAxis Axis,
	float AxisOffset,
	float Radius,
	uint32 Segments)
{
	check(Segments >= 3);
	for (uint32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Angle0 = KMath::Pi * 2.0f * static_cast<float>(Segment) / static_cast<float>(Segments);
		const float Angle1 = KMath::Pi * 2.0f * static_cast<float>(Segment + 1) / static_cast<float>(Segments);
		const float Cos0 = std::cos(Angle0) * Radius;
		const float Sin0 = std::sin(Angle0) * Radius;
		const float Cos1 = std::cos(Angle1) * Radius;
		const float Sin1 = std::sin(Angle1) * Radius;
		FVector Start;
		FVector End;
		switch (Axis)
		{
		case EAxis::X: Start = FVector(AxisOffset, Cos0, Sin0); End = FVector(AxisOffset, Cos1, Sin1); break;
		case EAxis::Y: Start = FVector(Cos0, AxisOffset, Sin0); End = FVector(Cos1, AxisOffset, Sin1); break;
		case EAxis::Z: Start = FVector(Cos0, Sin0, AxisOffset); End = FVector(Cos1, Sin1, AxisOffset); break;
		}
		AddMeshLine(Vertices, Indices, Start, End);
	}
}

void FDebugDraw::BuildCubeMesh(FGeometryMesh& Mesh)
{
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	static constexpr FVector Corners[] = {
		{ -1.0f, -1.0f, -1.0f }, { 1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, -1.0f }, { -1.0f, 1.0f, -1.0f },
		{ -1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f },
	};
	static constexpr uint8 Edges[][2] = {
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, { 4, 5 }, { 5, 6 },
		{ 6, 7 }, { 7, 4 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
	};
	for (const auto& Edge : Edges)
	{
		AddMeshLine(Vertices, Indices, Corners[Edge[0]], Corners[Edge[1]]);
	}
	Mesh.Initialize(Vertices, Indices);
}

void FDebugDraw::BuildSphereMesh(FGeometryMesh& Mesh)
{
	static constexpr uint32 Segments = 32;
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	AddCircle(Vertices, Indices, EAxis::X, 0.0f, 1.0f, Segments);
	AddCircle(Vertices, Indices, EAxis::Y, 0.0f, 1.0f, Segments);
	AddCircle(Vertices, Indices, EAxis::Z, 0.0f, 1.0f, Segments);
	Mesh.Initialize(Vertices, Indices);
}

void FDebugDraw::BuildHemisphereMesh(FGeometryMesh& Mesh)
{
	static constexpr uint32 Segments = 32;
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	AddCircle(Vertices, Indices, EAxis::Z, 0.0f, 1.0f, Segments);
	for (uint32 Arc = 0; Arc < 2; ++Arc)
	{
		for (uint32 Segment = 0; Segment < Segments / 2; ++Segment)
		{
			const float Angle0 = KMath::Pi * static_cast<float>(Segment) / static_cast<float>(Segments / 2);
			const float Angle1 = KMath::Pi * static_cast<float>(Segment + 1) / static_cast<float>(Segments / 2);
			const FVector Start = Arc == 0 ? FVector(std::cos(Angle0), 0.0f, std::sin(Angle0)) : FVector(0.0f, std::cos(Angle0), std::sin(Angle0));
			const FVector End = Arc == 0 ? FVector(std::cos(Angle1), 0.0f, std::sin(Angle1)) : FVector(0.0f, std::cos(Angle1), std::sin(Angle1));
			AddMeshLine(Vertices, Indices, Start, End);
		}
	}
	Mesh.Initialize(Vertices, Indices);
}

void FDebugDraw::BuildCylinderMesh(FGeometryMesh& Mesh)
{
	static constexpr uint32 Segments = 32;
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	AddCircle(Vertices, Indices, EAxis::Z, -1.0f, 1.0f, Segments);
	AddCircle(Vertices, Indices, EAxis::Z, 1.0f, 1.0f, Segments);
	for (uint32 Side = 0; Side < 4; ++Side)
	{
		const float Angle = KMath::HalfPi * static_cast<float>(Side);
		const FVector Radial(std::cos(Angle), std::sin(Angle), 0.0f);
		AddMeshLine(Vertices, Indices, Radial + FVector(0.0f, 0.0f, -1.0f), Radial + FVector(0.0f, 0.0f, 1.0f));
	}
	Mesh.Initialize(Vertices, Indices);
}

void FDebugDraw::BuildCapsuleMesh(FGeometryMesh& Mesh)
{
	static constexpr uint32 Segments = 32;
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	AddCircle(Vertices, Indices, EAxis::Z, -1.0f, 1.0f, Segments);
	AddCircle(Vertices, Indices, EAxis::Z, 1.0f, 1.0f, Segments);
	for (uint32 Side = 0; Side < 4; ++Side)
	{
		const float Angle = KMath::HalfPi * static_cast<float>(Side);
		const FVector Radial(std::cos(Angle), std::sin(Angle), 0.0f);
		AddMeshLine(Vertices, Indices, Radial + FVector(0.0f, 0.0f, -1.0f), Radial + FVector(0.0f, 0.0f, 1.0f));
	}
	for (uint32 Plane = 0; Plane < 2; ++Plane)
	{
		for (uint32 Segment = 0; Segment < Segments / 2; ++Segment)
		{
			const float Angle0 = KMath::Pi * static_cast<float>(Segment) / static_cast<float>(Segments / 2);
			const float Angle1 = KMath::Pi * static_cast<float>(Segment + 1) / static_cast<float>(Segments / 2);
			const float Radial0 = std::cos(Angle0);
			const float Radial1 = std::cos(Angle1);
			const float Height0 = std::sin(Angle0);
			const float Height1 = std::sin(Angle1);
			for (const float Sign : { -1.0f, 1.0f })
			{
				const FVector Start = Plane == 0 ? FVector(Radial0, 0.0f, Sign * (1.0f + Height0)) : FVector(0.0f, Radial0, Sign * (1.0f + Height0));
				const FVector End = Plane == 0 ? FVector(Radial1, 0.0f, Sign * (1.0f + Height1)) : FVector(0.0f, Radial1, Sign * (1.0f + Height1));
				AddMeshLine(Vertices, Indices, Start, End);
			}
		}
	}
	Mesh.Initialize(Vertices, Indices);
}

void FDebugDraw::BuildArrowMesh(FGeometryMesh& Mesh)
{
	static constexpr uint32 Segments = 8;
	static constexpr float HeadStart = 0.75f;
	static constexpr float HeadRadius = 0.08f;
	TArray<FGeometryVertex> Vertices;
	TArray<uint32> Indices;
	AddMeshLine(Vertices, Indices, FVector::ZeroVector, FVector::ForwardVector);
	AddCircle(Vertices, Indices, EAxis::X, HeadStart, HeadRadius, Segments);
	for (uint32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Angle = KMath::Pi * 2.0f * static_cast<float>(Segment) / static_cast<float>(Segments);
		AddMeshLine(Vertices, Indices, FVector::ForwardVector, FVector(HeadStart, std::cos(Angle) * HeadRadius, std::sin(Angle) * HeadRadius));
	}
	Mesh.Initialize(Vertices, Indices);
}

FMatrix FDebugDraw::MakeArrowTransform(const FVector& Start, const FVector& End)
{
	const FVector Direction = End - Start;
	const float Length = Direction.Size();
	const FVector Forward = Direction / Length;
	const FVector ReferenceUp = std::fabs(Forward | FVector::UpVector) > 0.99f ? FVector::RightVector : FVector::UpVector;
	const FVector Right = (ReferenceUp ^ Forward).GetSafeNormal();
	const FVector Up = Forward ^ Right;
	const FMatrix Rotation(
		Forward.X, Forward.Y, Forward.Z, 0.0f,
		Right.X, Right.Y, Right.Z, 0.0f,
		Up.X, Up.Y, Up.Z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
	return FMatrix::MakeWorld(Start, Rotation, FVector(Length, Length, Length));
}

void FDebugDraw::AddInstance(
	EShapeType ShapeType,
	const FMatrix& Model,
	const FColor& Color,
	const FVector4& ShapeParameters)
{
	Instances[static_cast<uint32>(ShapeType)].push_back({ Model, Color.ToVector4(), ShapeParameters });
}

const FGeometryMesh& FDebugDraw::GetShapeMesh(EShapeType ShapeType) const
{
	switch (ShapeType)
	{
	case EShapeType::Cube: return CubeMesh;
	case EShapeType::Sphere: return SphereMesh;
	case EShapeType::Hemisphere: return HemisphereMesh;
	case EShapeType::Cylinder: return CylinderMesh;
	case EShapeType::Capsule: return CapsuleMesh;
	case EShapeType::Arrow: return ArrowMesh;
	case EShapeType::Count: break;
	}
	check(false);
	return CubeMesh;
}

void FDebugDraw::PrepareLineBuffer(IRenderDevice& RenderDevice)
{
	check(LineVertices.size() <= (std::numeric_limits<uint32>::max)());
	const uint32 RequiredVertices = static_cast<uint32>(LineVertices.size());
	if (RequiredVertices == 0)
	{
		return;
	}
	if (RequiredVertices > LineBufferCapacity)
	{
		uint32 NewCapacity = std::max(256u, LineBufferCapacity);
		while (NewCapacity < RequiredVertices)
		{
			check(NewCapacity <= (std::numeric_limits<uint32>::max)() / 2);
			NewCapacity *= 2;
		}
		check(NewCapacity <= (std::numeric_limits<uint32>::max)() / sizeof(FGeometryVertex));
		if (LineBuffer.IsValid())
		{
			RenderDevice.DestroyBuffer(LineBuffer);
		}
		const FBufferDesc Desc = { NewCapacity * static_cast<uint32>(sizeof(FGeometryVertex)), EBufferUsage::Vertex, EResourceAccess::CPUWrite };
		LineBuffer = RenderDevice.CreateBuffer(Desc);
		LineBufferCapacity = NewCapacity;
	}

	TArray<FGeometryVertex> UploadVertices = LineVertices;
	UploadVertices.resize(LineBufferCapacity);
	const auto* Bytes = reinterpret_cast<const uint8*>(UploadVertices.data());
	RenderDevice.UpdateBuffer(LineBuffer, std::span<const uint8>(Bytes, UploadVertices.size() * sizeof(FGeometryVertex)));
}
