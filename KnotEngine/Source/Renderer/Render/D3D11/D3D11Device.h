#pragma once

#include <wrl/client.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
// D3D11 장치와 Immediate Context만 소유한다. 창/Swap Chain은 FD3D11RenderContext가 담당한다.
class FD3D11Device final
{
public:
	FD3D11Device();
	~FD3D11Device();

	void Create();
	void Release();

	void FlushAndUnbindTargets();

	ID3D11Device* GetDevice() const { return Device.Get(); }
	ID3D11DeviceContext* GetContext() const { return DeviceContext.Get(); }

private:
	Microsoft::WRL::ComPtr<ID3D11Device> Device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
};
