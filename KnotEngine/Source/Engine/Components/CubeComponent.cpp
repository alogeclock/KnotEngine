#include "Components/CubeComponent.h"

UCubeComponent::UCubeComponent(URenderer& Renderer)
{
	if (VertexBuffer)
	{
		return;
	}

    static FGeometryVertex vertices[] = {
        // Front
        { FVector(-1.0f, -1.0f, -1.0f), PackColor(255, 51, 51) },
        { FVector(-1.0f,  1.0f, -1.0f), PackColor(255, 51, 51) },
        { FVector( 1.0f,  1.0f, -1.0f), PackColor(255, 51, 51) },
        { FVector(-1.0f, -1.0f, -1.0f), PackColor(255, 51, 51) },
        { FVector( 1.0f,  1.0f, -1.0f), PackColor(255, 51, 51) },
        { FVector( 1.0f, -1.0f, -1.0f), PackColor(255, 51, 51) },

        // Back
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(51, 255, 77) },
        { FVector( 1.0f, -1.0f,  1.0f), PackColor(51, 255, 77) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(51, 255, 77) },
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(51, 255, 77) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(51, 255, 77) },
        { FVector(-1.0f,  1.0f,  1.0f), PackColor(51, 255, 77) },

        // Left
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(51, 115, 255) },
        { FVector(-1.0f,  1.0f,  1.0f), PackColor(51, 115, 255) },
        { FVector(-1.0f,  1.0f, -1.0f), PackColor(51, 115, 255) },
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(51, 115, 255) },
        { FVector(-1.0f,  1.0f, -1.0f), PackColor(51, 115, 255) },
        { FVector(-1.0f, -1.0f, -1.0f), PackColor(51, 115, 255) },

        // Right
        { FVector( 1.0f, -1.0f, -1.0f), PackColor(255, 217, 51) },
        { FVector( 1.0f,  1.0f, -1.0f), PackColor(255, 217, 51) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(255, 217, 51) },
        { FVector( 1.0f, -1.0f, -1.0f), PackColor(255, 217, 51) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(255, 217, 51) },
        { FVector( 1.0f, -1.0f,  1.0f), PackColor(255, 217, 51) },

        // Top
        { FVector(-1.0f,  1.0f, -1.0f), PackColor(230, 230, 51) },
        { FVector(-1.0f,  1.0f,  1.0f), PackColor(230, 230, 51) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(230, 230, 51) },
        { FVector(-1.0f,  1.0f, -1.0f), PackColor(230, 230, 51) },
        { FVector( 1.0f,  1.0f,  1.0f), PackColor(230, 230, 51) },
        { FVector( 1.0f,  1.0f, -1.0f), PackColor(230, 230, 51) },

        // Bottom
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(217, 64, 255) },
        { FVector(-1.0f, -1.0f, -1.0f), PackColor(217, 64, 255) },
        { FVector( 1.0f, -1.0f, -1.0f), PackColor(217, 64, 255) },
        { FVector(-1.0f, -1.0f,  1.0f), PackColor(217, 64, 255) },
        { FVector( 1.0f, -1.0f, -1.0f), PackColor(217, 64, 255) },
        { FVector( 1.0f, -1.0f,  1.0f), PackColor(217, 64, 255) },
    };

    VertexCount = ARRAYSIZE(vertices);
    VertexBuffer = Renderer.CreateVertexBuffer(vertices, sizeof(vertices));
}
