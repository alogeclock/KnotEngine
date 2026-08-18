#pragma once

#include <wrl/client.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

// D3D11 장치와 Immediate Context의 생성·소유·해제를 담당하는 객체.
// - ID3D11Device는 GPU 자원을 생성하고, ID3D11DeviceContext는 렌더링 명령과 파이프라인 상태를 처리한다.
// - IDXGISwapChain은 화면에 표시할 Back Buffer들을 관리하며, Create()가 생성해 호출자에게 반환한다.
class FD3D11Device final
{
public:
	Microsoft::WRL::ComPtr<IDXGISwapChain> Create(void* NativeWindowHandle);
	void Release();

	void FlushAndUnbindTargets();

	ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetContext() const { return DeviceContext.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D11Device> Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};
