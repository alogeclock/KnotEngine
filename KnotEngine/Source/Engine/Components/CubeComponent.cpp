#include "Components/CubeComponent.h"

#include "Core/Assert.h"
#include "Core/Math/Vector.h"
#include "Render/Resource/MeshTypes.h"

#include <memory>
#include <span>

UCubeComponent::UCubeComponent(URenderer& Renderer)
{
	static const FGeometryVertex Vertices[] = {
		// Front
		{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(255, 51, 51, 255) },
		{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(255, 51, 51, 255) },
		{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(255, 51, 51, 255) },
		{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(255, 51, 51, 255) },

		// Back
		{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(51, 255, 77, 255) },
		{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(51, 255, 77, 255) },
		{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(51, 255, 77, 255) },
		{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(51, 255, 77, 255) },

		// Left
		{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(51, 115, 255, 255) },
		{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(51, 115, 255, 255) },
		{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(51, 115, 255, 255) },
		{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(51, 115, 255, 255) },

		// Right
		{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(255, 217, 51, 255) },
		{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(255, 217, 51, 255) },
		{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(255, 217, 51, 255) },
		{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(255, 217, 51, 255) },

		// Top
		{ FVector(-1.0f, 1.0f, -1.0f), PackRGBA(230, 230, 51, 255) },
		{ FVector(-1.0f, 1.0f, 1.0f), PackRGBA(230, 230, 51, 255) },
		{ FVector(1.0f, 1.0f, 1.0f), PackRGBA(230, 230, 51, 255) },
		{ FVector(1.0f, 1.0f, -1.0f), PackRGBA(230, 230, 51, 255) },

		// Bottom
		{ FVector(-1.0f, -1.0f, 1.0f), PackRGBA(217, 64, 255, 255) },
		{ FVector(-1.0f, -1.0f, -1.0f), PackRGBA(217, 64, 255, 255) },
		{ FVector(1.0f, -1.0f, -1.0f), PackRGBA(217, 64, 255, 255) },
		{ FVector(1.0f, -1.0f, 1.0f), PackRGBA(217, 64, 255, 255) },
	};

	static const uint32 Indices[] = {
		0,
		1,
		2,
		0,
		2,
		3,
		4,
		5,
		6,
		4,
		6,
		7,
		8,
		9,
		10,
		8,
		10,
		11,
		12,
		13,
		14,
		12,
		14,
		15,
		16,
		17,
		18,
		16,
		18,
		19,
		20,
		21,
		22,
		20,
		22,
		23,
	};

	Mesh = std::make_shared<FGeometryMesh>();
	Mesh->SetData(std::span(Vertices), std::span(Indices));
	panicf(Mesh->Upload(Renderer), "기본 Cube Mesh의 GPU 업로드 실패.");
}
