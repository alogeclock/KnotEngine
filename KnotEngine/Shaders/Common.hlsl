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

cbuffer FrameBuffer : register(b0)
{
	row_major float4x4 ModelViewProjection;
};

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    float4 Color;
};

PS_INPUT VS(VS_INPUT input)
{
	PS_INPUT output;
	
    output.position = mul(float4(input.position, 1.0f), ModelViewProjection);
	output.color = input.color;
	
	return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
	return input.color;
}
