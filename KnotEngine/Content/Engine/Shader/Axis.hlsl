// 정점 버퍼 없이 World 원점부터 양의 X/Y/Z 방향으로 뻗는 축을 그린다.
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Color : COLOR;
};

Texture2D<float> SelectionDepth : register(t0);

// b0은 View마다 갱신하는 공용 상수 슬롯이다.
cbuffer ViewConstants : register(b0)
{
    row_major float4x4 ViewProjection;
    row_major float4x4 InverseViewProjection;
    float3 ViewOrigin;
    float FarClip;
};

// b1은 Pass마다 한 번 갱신하는 공용 상수 슬롯이다.
cbuffer OverlayConstants : register(b1)
{
    uint HasSelection;
    float GridSpacing;
    float MajorGridInterval;
    uint Padding;

    float4 MinorColor;
    float4 MajorColor;
};

static const float FadeDistance = 20000.0f;

VS_OUTPUT VS(uint VertexId : SV_VertexID)
{
    uint AxisIndex = VertexId / 2;
    float3 AxisDirection = AxisIndex == 0 ? float3(1.0f, 0.0f, 0.0f) : AxisIndex == 1 ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);

    // View를 중심으로 한 Fade 구와 양의 축이 만나는 가장 먼 지점을 선의 끝으로 사용한다.
    float FadeEnd = min(FadeDistance, FarClip * 0.95f);
    float ViewAlongAxis = dot(ViewOrigin, AxisDirection);
    float ViewDistSq = dot(ViewOrigin, ViewOrigin);
    float PerpendicularDistSq = max(0.0f, ViewDistSq - ViewAlongAxis * ViewAlongAxis);
    float IntersectionDistSq = max(0.0f, FadeEnd * FadeEnd - PerpendicularDistSq);
    float AxisEnd = max(0.0f, ViewAlongAxis + sqrt(IntersectionDistSq));

    float AxisAmount = (VertexId & 1) != 0 ? AxisEnd : 0.0f;
    float3 WorldPosition = AxisDirection * AxisAmount;

    VS_OUTPUT Output;
    Output.Position = mul(float4(WorldPosition, 1.0f), ViewProjection);
    Output.WorldPosition = WorldPosition;
    Output.Color = AxisDirection;
    return Output;
}

float4 PS(VS_OUTPUT Input) : SV_TARGET
{
    float FadeStart = min(FadeDistance * 0.5f, FarClip * 0.8f);
    float FadeEnd = min(FadeDistance, FarClip * 0.95f);
    float ViewDistance = length(Input.WorldPosition - ViewOrigin);
    float Alpha = 1.0f - smoothstep(FadeStart, FadeEnd, ViewDistance);
    if (Alpha <= 0.0f)
    {
        discard;
    }
    // 선택 물체의 2 Pixel Outline 위치에서 물체보다 뒤에 있는 Axis만 버린다.
    if (HasSelection != 0)
    {
        uint Width;
        uint Height;
        SelectionDepth.GetDimensions(Width, Height);
        const int2 Pixel = int2(Input.Position.xy);
        const int2 MaxPixel = int2(Width, Height) - 1;
        if (SelectionDepth.Load(int3(clamp(Pixel, 0, MaxPixel), 0)) == 0.0f)
        {
            static const int2 NeighborOffsets[8] =
            {
                int2(-1, -1), int2(0, -1), int2(1, -1),
                int2(-1, 0),               int2(1, 0),
                int2(-1, 1),  int2(0, 1),  int2(1, 1),
            };

            float OutlineDepth = 0.0f;
            [unroll]
            for (uint Index = 0; Index < 8; ++Index)
            {
                const int2 SamplePixel = clamp(Pixel + NeighborOffsets[Index] * 2, 0, MaxPixel);
                OutlineDepth = max(OutlineDepth, SelectionDepth.Load(int3(SamplePixel, 0)));
            }
            if (OutlineDepth > Input.Position.z)
            {
                discard;
            }
        }
    }

    return float4(Input.Color, Alpha);
}
