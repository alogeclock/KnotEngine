// 위치와 정점 색상만 사용하는 기본 Opaque Mesh 셰이더다.
struct VS_INPUT
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

// b0은 View마다 갱신하는 공용 상수 슬롯이다.
cbuffer ViewConstants : register(b0)
{
	row_major float4x4 ViewProjection;
	row_major float4x4 InverseViewProjection;
	float3 ViewOrigin;
	float ViewPadding;
};

// b3은 Object Draw마다 갱신하는 상수 슬롯이다.
cbuffer DrawConstants : register(b3)
{
	row_major float4x4 Model;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output;

	float4 World = mul(float4(input.position, 1.0f), Model);
	output.position = mul(World, ViewProjection);
	output.color = input.color;

	return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}
