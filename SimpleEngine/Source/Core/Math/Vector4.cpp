#include "Vector4.h"
#include "Matrix.h"

inline FVector4 FVector4::operator*(const FMatrix& Mat) const noexcept
{
	const DirectX::XMVECTOR R = DirectX::XMVector4Transform(ToXMVector(), Mat.ToXMMatrix());
	DirectX::XMFLOAT4 T;
	DirectX::XMStoreFloat4(&T, R);
	return FVector4(T.x, T.y, T.z, T.w);
}
