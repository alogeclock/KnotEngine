struct VS_INPUT
{
	float3 Position : POSITION;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
};

// b0은 View마다 갱신하는 공용 상수 슬롯이다.
cbuffer ViewConstants : register(b0)
{
	row_major float4x4 ViewProjection;
	row_major float4x4 InverseViewProjection;
	float3 ViewOrigin;
	float FarClip;
};

// b3은 Object Draw마다 갱신하는 상수 슬롯이다.
cbuffer DrawConstants : register(b3)
{
	row_major float4x4 Model;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output;
	
	float4 WorldPosition = mul(float4(input.Position, 1.0f), Model);
	output.Position = mul(WorldPosition, ViewProjection);
	
	return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
	return float4(0.8f, 0.8f, 0.8f, 1.0f);
}
