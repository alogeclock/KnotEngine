#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
};

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

FVertexSimple triangle_vertices[] =
{
	{  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
	{  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
	{ -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
};

FVertexSimple cube_vertices[] =
{
	// Front face (Z+)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Bottom-left (red)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-right (blue)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

	// Back face (Z-)
	{ -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-left (cyan)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
	{  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)

	// Left face (X-)
	{ -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-left (purple)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

	// Right face (X+)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f }, // Bottom-left (orange)
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.5f, 1.0f }, // Top-right (dark blue)

	// Top face (Y+)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.5f, 1.0f }, // Bottom-left (light green)
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
	{  0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Top-right (brown)
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)

	// Bottom face (Y-)
	{ -0.5f, -0.5f, -0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Bottom-left (brown)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Top-right (green)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
};

#include "Sphere.h"

class UPrimitive
{
public:
	virtual void Update(float t) = 0; // Frame Rate 제한을 풀어도 공의 속도가 변할 뿐 그 외에는 정상적으로 동작해야 한다.
	virtual void RenderPrimitive(URenderer& renderer) = 0;
	virtual bool DetectCollision(UPrimitive* other) = 0;
	virtual void ResolveCollision(UPrimitive* other) = 0;
	virtual void SetVelocity(const FVector& v) = 0;

	virtual ~UPrimitive() {}
};

class UBall : public UPrimitive
{
public:
	FVector Location;
	FVector Velocity;
	float Radius;
	float Mass;

	static int TotalNumBalls;
	static bool bGravity;
	static bool bBlackhole;
	static float BlackholeStrength;

	static UINT numVerticesSphere;
	static ID3D11Buffer* SphereVertexBuffer;

public:
	UBall()
	{
		TotalNumBalls++;

		Radius = ((float)(rand() % 10) / 100.0f) + 0.05f; // 0.05 ~ 0.15

		Mass = Radius * 20.0f;

		Location.x = ((float)(rand() % 160) / 100.0f) - 0.8f; // -0.8 ~ 0.8
		Location.y = ((float)(rand() % 160) / 100.0f) - 0.8f; // -0.8 ~ 0.8
		Location.z = 0.0f;

		float speedMod = ((float)(rand() % 10) / 10000.0f) + 0.001f; // 0.001f ~ 0.002f
		Velocity.x = ((float)(rand() % 100 - 50)) * speedMod;
		Velocity.y = ((float)(rand() % 100 - 50)) * speedMod;
		Velocity.z = 0.0f;
	}

	~UBall() override
	{
		TotalNumBalls--;
	}

	// Frame Rate 제한을 풀어도 공의 속도가 변할 뿐 그 외에는 정상적으로 동작해야 한다.
	virtual void Update(float gravity) override
	{
		if (bGravity)
		{
			Velocity.y -= gravity;
		}

		if (bBlackhole)
		{
			float dx = 0.0f - Location.x;
			float dy = 0.0f - Location.y;
			float dst = sqrt(dx * dx + dy * dy);

			if (dst > 0.01f)
			{
				Velocity.x += (dx / dst) * BlackholeStrength;
				Velocity.y += (dy / dst) * BlackholeStrength;
			}
		}

		Location.x += Velocity.x;
		Location.y += Velocity.y;
		Location.z += Velocity.z;

		if (Location.x < -1.0f + Radius)
		{
			Location.x = -1.0f + Radius;
			Velocity.x *= -1.0f;
		}
		if (Location.x > 1.0f - Radius)
		{
			Location.x = 1.0f - Radius;
			Velocity.x *= -1.0f;
		}
		if (Location.y < -1.0f + Radius)
		{
			Location.y = -1.0f + Radius;
			Velocity.y *= -1.0f;
		}
		if (Location.y > 1.0f - Radius)
		{
			Location.y = 1.0f - Radius;
			Velocity.y *= -1.0f;
		}
	}

	virtual void RenderPrimitive(URenderer& renderer) override
	{
		renderer.UpdateConstant(Location, Radius);
		if (SphereVertexBuffer != nullptr)
		{
			renderer.RenderPrimitive(SphereVertexBuffer, numVerticesSphere);
		}
	}

	virtual bool DetectCollision(UPrimitive* other) override
	{
		UBall* otherBall = (UBall*)other; // downcasting
		if (otherBall == nullptr) return false;

		float dx = Location.x - otherBall->Location.x;
		float dy = Location.y - otherBall->Location.y;

		float dstSquared = dx * dx + dy * dy;
		float sumRadius = Radius + otherBall->Radius;

		return (dstSquared <= sumRadius * sumRadius);
	}

	/* 
		UBall에 대해 elastic collision을 구현한다.
		1. 충돌을 감지한다. (DetectCollision에서 수행)
		2. 충돌 직전 위치로 재조정한다.
		3. 충돌 전 속도를 공의 중심을 잇는 방향 성분(normal), 충돌면의 접선(tangent) 성분으로 분해한다.
		4. normal 성분끼리만 충돌 공식을 적용하여 충돌 후 속도를 구한다.
		5. 구한 충돌 후 속도와, tangent 속도를 벡터합한다.
	*/

	virtual void ResolveCollision(UPrimitive* other) override
	{
		UBall* otherBall = (UBall*)other; // downcasting
		if (otherBall == nullptr) return;

		// 2. 충돌 직전 위치로 재조정한다.
		float dx = Location.x - otherBall->Location.x;
		float dy = Location.y - otherBall->Location.y;
		float dst = sqrt(dx * dx + dy * dy);

		float m1 = Mass;
		float m2 = otherBall->Mass;
		FVector v1 = Velocity;
		FVector v2 = otherBall->Velocity;

		if (dst == 0.0f) return; // 0으로 나누기를 방지

		float sumRadius = Radius + otherBall->Radius;
		float overlap = sumRadius - dst;
		
		if (overlap > 0)
		{
			FVector correctionVector;
			correctionVector.x = (dx / dst) * overlap;
			correctionVector.y = (dy / dst) * overlap;
			
			// 공의 크기에 따라 위치를 재조정한다. (클수록 덜 밀려남)
			Location.x += correctionVector.x * (m2 / (m1 + m2));
			Location.y += correctionVector.y * (m2 / (m1 + m2));
			otherBall->Location.x -= correctionVector.x * (m1 / (m1 + m2));
			otherBall->Location.y -= correctionVector.y * (m1 / (m1 + m2));
		}

		// 3. 충돌 전 속도를 공의 중심을 잇는 방향 성분(normal), 충돌면의 접선(tangent) 성분으로 분해한다.
		// 내적을 활용해 분해한다.
		FVector normal;
		normal.x = dx / dst;
		normal.y = dy / dst;

		FVector tangent;
		tangent.x = -normal.y;
		tangent.y = normal.x;

		float velAlongNormal = (v1.x - v2.x) * normal.x + (v1.y - v2.y) * normal.y;

		// velAlongNormal > 0 이라면, 두 공은 이미 서로 멀어지는 중이므로 계산하지 않는다.
		if (velAlongNormal > 0) return;

		// 속도를 성분별로 tangent, normal 값으로 분해한다.
		float v1n = v1.x * normal.x + v1.y * normal.y;   // Normal 방향 속도 크기
		float v1t = v1.x * tangent.x + v1.y * tangent.y; // Tangent 방향 속도 크기
		float v2n = v2.x * normal.x + v2.y * normal.y;
		float v2t = v2.x * tangent.x + v2.y * tangent.y;

		float e = 0.0f; // (반발 계수 e: 1.0 = 완전 탄성)
		
		float v1n_new = ((m1 - e * m2) * v1n + (1 + e) * m2 * v2n) / (m1 + m2);
		float v2n_new = ((m2 - e * m1) * v2n + (1 + e) * m1 * v1n) / (m1 + m2);

		v1.x = v1n_new * normal.x + v1t * tangent.x;
		v1.y = v1n_new * normal.y + v1t * tangent.y;
		v2.x = v2n_new * normal.x + v2t * tangent.x;
		v2.y = v2n_new * normal.y + v2t * tangent.y;

		SetVelocity(v1);
		other->SetVelocity(v2);
	}

	virtual void SetVelocity(const FVector& v) override
	{
		Velocity = v;
	}
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// WndProc 함수는 각종 메시지를 처리한다.
/* hWnd는 이벤트가 발생한 창의 번호를 의미한다. message는 사건의 종류를 의미한다. 
   가령 WM_KEYDOWN은 키 눌림, WM_LBUTTONDOWN은 마우스 클릭, 
	 WM_DESTROY는 창 파괴를 의미한다. */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0); // 프로그램 종료 메시지를 메시지 큐에 넣는다.
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}	

enum ETypePrimitive
{
	EPT_Triangle,
	EPT_Cube,
	EPT_Sphere,
	EPT_Max,
};

ETypePrimitive typePrimitive = EPT_Sphere;
ID3D11Buffer* UBall::SphereVertexBuffer = nullptr;
int UBall::TotalNumBalls = 0;
UINT UBall::numVerticesSphere = 0;
bool UBall::bGravity = true;
bool UBall::bBlackhole = false;
float UBall::BlackholeStrength = 0.01f;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	// wndProc의 함수 포인터를 WindowClass 구조체 안에 넣는다.
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	URenderer renderer;
	renderer.Create(hWnd);
	renderer.CreateShader();
	renderer.CreateConstantBuffer();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(renderer.Device, renderer.DeviceContext);

	// UINT numVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);
	// UINT numVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
	UBall::numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	UBall::SphereVertexBuffer = vertexBufferSphere;

	int currentBallCount = 0;
	int targetBallCount = 1;
	int listSize = 16;
	UPrimitive** PrimitiveList = new UPrimitive*[listSize];
	
	PrimitiveList[currentBallCount++] = new UBall();

	const float leftBorder = -1.0f;
	const float rightBorder = 1.0f;
	const float topBorder = -1.0f;
	const float bottomBorder = 1.0f;
	const float sphereRadius = 1.0f;

	bool bIsExit = false;

	// Timing 관련 코드
	const int targetFPS = 30;
	const double targetFrameTime = 1000.0 / targetFPS;

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);

	LARGE_INTEGER startTime, endTime;
	double elapsedTime = 0.0;

	while (bIsExit == false)
	{
		QueryPerformanceCounter(&startTime);

		MSG msg;
		// WinMain 함수의 message loop, GetMessage가 WM_QUIT을 메시지 큐에서 발견하면 false를 반환하고 종료한다.
		// 처리할 메시지가 없을 때까지 수행한다. non-blocking 명령어이다.
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg); // 키 입력 메시지를 번역한다.
			DispatchMessage(&msg);  // 메시지가 WndProc으로 전달된다.

			if (msg.message == WM_QUIT)
			{
				bIsExit = true;
				break;
			}
		}

		if (targetBallCount < 1) targetBallCount = 1;

		// 공 생성
		while (currentBallCount < targetBallCount)
		{
			// 리스트 크기가 부족할 경우, PrimitiveList의 크기를 증가시킨 뒤 공 생성
			if (currentBallCount >= listSize)
			{
				int newListSize = listSize * 2;
				UPrimitive** NewPrimitiveList = new UPrimitive*[newListSize];
				
				for (int i = 0; i < currentBallCount; i++)
				{
					NewPrimitiveList[i] = PrimitiveList[i];
				}

				delete[] PrimitiveList;
				PrimitiveList = NewPrimitiveList;
				listSize = newListSize;
			}
			PrimitiveList[currentBallCount++] = new UBall();
		}
		
		// 공 삭제
		while (currentBallCount > targetBallCount)
		{
			// 리스트 크기를 줄이는 경우는 고려하지 않음 (재할당 X)
			// 공을 무작위로 뽑은 뒤 삭제하고, 마지막 공과 인덱스 교체
			int idx = rand() % currentBallCount;
			delete PrimitiveList[idx];
			PrimitiveList[idx] = PrimitiveList[currentBallCount - 1];
			PrimitiveList[currentBallCount - 1] = nullptr;
			currentBallCount--;
		}

		// 물리 업데이트 (중력, 벽 충돌)
		for (int i = 0; i < currentBallCount; i++)
		{
			PrimitiveList[i]->Update(0.005f);
		}

		// Brute-Force 충돌 처리
		for (int i = 0; i < currentBallCount; i++)
		{
			for (int j = i + 1; j < currentBallCount; j++)
			{
				if (PrimitiveList[i]->DetectCollision(PrimitiveList[j]))
				{
					PrimitiveList[i]->ResolveCollision(PrimitiveList[j]);
				}
			}
		}

		renderer.Prepare();
		renderer.PrepareShader();

		for (int i = 0; i < currentBallCount; i++)
		{
			PrimitiveList[i]->RenderPrimitive(renderer);
		}

		/* ImGui 렌더링 요청 */

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		/* ImGui 컨트롤 추가 */

		ImGui::Begin("Jungle Property Window");
		ImGui::Text("Hello, Jungle!");

		if (ImGui::InputInt("Ball Count", &targetBallCount))
		{
			if (targetBallCount < 1) targetBallCount = 1;
		}

		ImGui::Checkbox("Gravity", &UBall::bGravity);
		ImGui::Checkbox("Blackhole", &UBall::bBlackhole);

		if (UBall::bBlackhole)
		{
			ImGui::SliderFloat("Blackhole Strength", &UBall::BlackholeStrength, 0.001f, 0.02f);
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		renderer.SwapBuffer();

		do
		{
			Sleep(0);
			QueryPerformanceCounter(&endTime);
			elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;
		} while (elapsedTime < targetFrameTime);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	for (int i = 0; i < currentBallCount; i++)
	{
		delete PrimitiveList[i];
	}
	delete[] PrimitiveList;

	renderer.ReleaseVertexBuffer(vertexBufferSphere);
	renderer.ReleaseConstantBuffer();

	renderer.ReleaseShader();
	renderer.Release();

	return 0;
}