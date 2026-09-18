struct VS_INPUT
{
	float3 Position : POSITION;
	float4 Color : COLOR;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
};

cbuffer ViewConstants : register(b0)
{
	row_major float4x4 ViewProjection;
	row_major float4x4 InverseViewProjection;
	float3 ViewOrigin;
	float FarClip;
};

struct FDebugInstance
{
	row_major float4x4 Model;
	float4 Color;
	float4 ShapeParameters;
};

cbuffer InstanceConstants : register(b3)
{
	FDebugInstance Instances[128];
};

PS_INPUT LineVS(VS_INPUT input)
{
	PS_INPUT output;
	output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
	output.Color = input.Color;
	return output;
}

PS_INPUT InstanceVS(VS_INPUT input, uint instanceId : SV_InstanceID)
{
	FDebugInstance instance = Instances[instanceId];
	float3 localPosition = input.Position;
	if (instance.ShapeParameters.z > 0.5f)
	{
		const float radius = instance.ShapeParameters.x;
		const float halfHeight = instance.ShapeParameters.y;
		localPosition.xy *= radius;
		localPosition.z = sign(localPosition.z) * (halfHeight + max(abs(localPosition.z) - 1.0f, 0.0f) * radius);
	}

	const float4 worldPosition = mul(float4(localPosition, 1.0f), instance.Model);
	PS_INPUT output;
	output.Position = mul(worldPosition, ViewProjection);
	output.Color = instance.Color;
	return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
	return input.Color;
}
