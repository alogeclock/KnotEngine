Texture2D SceneColor : register(t0);

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

// 정점 버퍼 없이 출력 Viewport 전체를 덮는 삼각형을 만든다.
VS_OUTPUT VS(uint VertexId : SV_VertexID)
{
	VS_OUTPUT Output;
	float2 Position = float2((VertexId == 1) ? 3.0f : -1.0f, (VertexId == 2) ? 3.0f : -1.0f);
	Output.Position = float4(Position, 0.0f, 1.0f);
	return Output;
}

float3 LinearToSRGB(float3 Color)
{
	Color = max(Color, 0.0f);
	float3 Lower = Color * 12.92f;
	float3 Upper = 1.055f * pow(Color, 1.0f / 2.4f) - 0.055f;
	return lerp(Upper, Lower, step(Color, 0.0031308f));
}

float4 PSGammaCorrection(VS_OUTPUT Input) : SV_TARGET
{
	float4 Color = SceneColor.Load(int3(uint2(Input.Position.xy), 0));
	return float4(LinearToSRGB(Color.rgb), Color.a);
}
