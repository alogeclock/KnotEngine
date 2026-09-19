Texture2D SceneColor : register(t0);
Texture2D<float> SelectionDepth : register(t1);

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

float4 PS(VS_OUTPUT Input) : SV_TARGET
{
	// Selection Outline
	uint Width;
	uint Height;
	SelectionDepth.GetDimensions(Width, Height);

	const int2 Pixel = int2(Input.Position.xy);
	const int2 MaxPixel = int2(Width, Height) - 1;
	const float CenterMask = SelectionDepth.Load(int3(clamp(Pixel, 0, MaxPixel), 0)) > 0.0f ? 1.0f : 0.0f;
	static const int2 NeighborOffsets[8] =
	{
		int2(-1, -1), int2(0, -1), int2(1, -1),
		int2(-1, 0),               int2(1, 0),
		int2(-1, 1),  int2(0, 1),  int2(1, 1),
	};

	float NeighborMask = 0.0f;
	[unroll]
	for (uint Index = 0; Index < 8; ++Index)
	{
		const int2 SamplePixel = clamp(Pixel + NeighborOffsets[Index] * 2, 0, MaxPixel);
		const float NeighborDepth = SelectionDepth.Load(int3(SamplePixel, 0));
		NeighborMask = max(NeighborMask, NeighborDepth > 0.0f ? 1.0f : 0.0f);
	}

	float4 Color = SceneColor.Load(int3(Pixel, 0));
	static const float3 SelectionColor = float3(1.0f, 0.35f, 0.0f);
	const float OuterOutline = saturate(NeighborMask - CenterMask);
	Color.rgb = lerp(Color.rgb, SelectionColor, OuterOutline);

	// Gamma Correction
	return float4(LinearToSRGB(Color.rgb), Color.a);
}
