#include "ViewportClient.h"

// 파생 뷰포트 클라이언트를 기반 인터페이스를 통해 안전하게 소멸시킨다.
FViewportClient::~FViewportClient() = default;

void FViewportClient::Tick(float DeltaTime)
{
	(void)DeltaTime;
}
