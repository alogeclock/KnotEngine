struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float2 TexCoord : TEXCOORD0;
};

struct INSTANCED_VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float3 Tangent : TANGENT;
	float2 TexCoord : TEXCOORD0;

	float4 Model0 : INSTANCE_MODEL0;
	float4 Model1 : INSTANCE_MODEL1;
	float4 Model2 : INSTANCE_MODEL2;
	float4 Model3 : INSTANCE_MODEL3;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
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

// b2는 Material Asset 값이 Shader Reflection Layout에 따라 패킹되는 상수 슬롯이다.
cbuffer MaterialConstants : register(b2)
{
	float4 BaseColor;
	float AlphaCutoff;
};

Texture2D BaseColorTexture : register(t0);
SamplerState BaseColorTextureSampler : register(s0);

PS_INPUT MainVS(VS_INPUT input)
{
	PS_INPUT output;
	
	float4 WorldPosition = mul(float4(input.Position, 1.0f), Model);
	output.Position = mul(WorldPosition, ViewProjection);
	output.TexCoord = input.TexCoord;
	
	return output;
}

PS_INPUT InstancedVS(INSTANCED_VS_INPUT input)
{
	PS_INPUT output;
	row_major float4x4 ModelMatrix = float4x4(input.Model0, input.Model1, input.Model2, input.Model3);
	float4 WorldPosition = mul(float4(input.Position, 1.0f), ModelMatrix);
	output.Position = mul(WorldPosition, ViewProjection);
	output.TexCoord = input.TexCoord;
	return output;
}

float4 GetBaseColor(PS_INPUT input)
{
	return BaseColor * BaseColorTexture.Sample(BaseColorTextureSampler, input.TexCoord);
}

float4 OpaquePS(PS_INPUT input) : SV_TARGET
{
	return GetBaseColor(input);
}

float4 MaskedPS(PS_INPUT input) : SV_TARGET
{
	float4 Color = GetBaseColor(input);
	clip(Color.a - AlphaCutoff);
	return Color;
}

float4 TranslucentPS(PS_INPUT input) : SV_TARGET
{
	return GetBaseColor(input);
}
