#pragma once

#include <Windows.h>
#include <cstdint>

class IImGuiRenderBackend;

class FEditorUISystem
{
public:
	explicit FEditorUISystem(IImGuiRenderBackend& InRenderBackend);

	void Startup(HWND WindowHandle);
	void BeginFrame();
	void Draw(float DeltaTime);
	void EndFrame();
	void Shutdown();

private:
	IImGuiRenderBackend& RenderBackend;
	float ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	
	// TO-DO: 프레임 통계는 별도 Overlay Panel로 분리하여 콘솔을 통해 출력할 수 있도록 한다.
	float ElapsedTime = 0.0f;
	std::uint32_t FrameCount = 0;

	float DisplayedFramesPerSecond = 0.0f;
	float DisplayedFrameTimeMs = 0.0f;
};
