struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

cbuffer ViewConstants : register(b0)
{
	row_major float4x4 ViewProjection;
	row_major float4x4 InverseViewProjection;
	float3 ViewOrigin;
	float FarClip;
};

cbuffer DrawConstants : register(b3)
{
	row_major float4x4 Model;
};

VS_OUTPUT VS(VS_INPUT Input)
{
	VS_OUTPUT Output;
	Output.Position = mul(mul(float4(Input.Position, 1.0f), Model), ViewProjection);
	return Output;
}

// Color 출력 없이 Rasterization된 선택 Geometry의 Depth만 기록한다.
void PS(VS_OUTPUT Input)
{
}
