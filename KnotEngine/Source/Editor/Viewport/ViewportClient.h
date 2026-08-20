#pragma once

class FViewportClient
{
public:
	virtual ~FViewportClient();

	virtual void Tick(float DeltaTime);
};
