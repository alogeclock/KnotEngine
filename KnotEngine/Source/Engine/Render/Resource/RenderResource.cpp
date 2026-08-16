#include "Render/Resource/RenderResource.h"

void FRenderResource::Release()
{
	if (!bInitialized)
	{
		return;
	}

	OnRelease();
	bInitialized = false;
}
