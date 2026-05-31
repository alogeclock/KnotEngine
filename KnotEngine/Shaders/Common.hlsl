struct VS_INPUT
{
	float4 position : POSITION;
	float4 color : COLOR;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

cbuffer constants : register(b0)
{
	row_major float4x4 MVP;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output;
	
	float3 finalPos = input.position.xyz;
	output.position = float4(finalPos, 1.0f);
	output.color = input.color;
	
	return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}
