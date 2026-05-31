#include "Render/Renderer.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

// ──────────── Microsoft 소스코드 주석 시그니쳐 ────────────
// _In_: 데이터 읽기만 허용, NULL 반환 금지
// _In_opt_: _In_과 동일 사양, NULL 전달 허용
// ────────────────────────────────────────────────────────
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
	WCHAR WindowClass[] = L"KnotWindowClass";
	WCHAR Title[] = L"KnotEngine - ImGui Window";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
		nullptr, nullptr, hInstance, nullptr);

	ShowWindow(hWnd, nShowCmd);

	URenderer renderer;
	renderer.Create(hWnd);

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.GetDevice(), renderer.GetDeviceContext());

	bool bIsExit = false;
	MSG msg;

	while (!bIsExit)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
			}
		}

		// 렌더링 준비
		renderer.Prepare();

		// ImGui 프레임 시작
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// ImGui 테스트 창
		ImGui::Begin("KnotEngine Property Window");
		ImGui::Text("Hello, KnotEngine!");
		ImGui::Separator();
		static float clear_color[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
		ImGui::ColorEdit4("Background Color", clear_color);
		ImGui::End();

		// ImGui 렌더링
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// 화면 출력
		renderer.SwapBuffer();
	}

	// 정리
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	renderer.Release();

	return 0;
}
