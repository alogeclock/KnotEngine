// 화면 전체 삼각형에서 광선을 복원하여 World Z=0 평면에 편집용 Grid를 그린다.
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 NdcPosition : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_TARGET;
    float Depth : SV_DEPTH;
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

static const float GridLineWidth = 1.0f;
static const float FadeDistance = 20000.0f;

// 정점 버퍼 없이 현재 Viewport 전체를 덮는 삼각형을 만든다.
VS_OUTPUT VS(uint VertexId : SV_VertexID)
{
    VS_OUTPUT Output;
    float2 Position = float2((VertexId == 1) ? 3.0f : -1.0f, (VertexId == 2) ? 3.0f : -1.0f);
    Output.Position = float4(Position, 0.0f, 1.0f);
    Output.NdcPosition = Position;
    return Output;
}

PS_OUTPUT PS(VS_OUTPUT Input)
{
    // Reversed-Z의 Near=1에서 시작하고 정확한 Far=0 대신 중간 깊이 지점으로 광선 방향만 복원한다.
    float4 NearPosition = mul(float4(Input.NdcPosition, 1.0f, 1.0f), InverseViewProjection);
    float4 RayPosition = mul(float4(Input.NdcPosition, 0.5f, 1.0f), InverseViewProjection);
    NearPosition /= NearPosition.w;
    RayPosition /= RayPosition.w;

    // World Z=0 평면과 평행하거나 View 앞에서 평면과 만나지 않는 광선은 버린다.
    float3 RayDirection = RayPosition.xyz - NearPosition.xyz;
    if (abs(RayDirection.z) < 0.00001f)
    {
        discard;
    }

    float RayDistance = -NearPosition.z / RayDirection.z;
    if (RayDistance < 0.0f)
    {
        discard;
    }

    float3 WorldPosition = NearPosition.xyz + RayDirection * RayDistance;
    float MajorGridSpacing = GridSpacing * MajorGridInterval;

    // 화면 미분값으로 선의 픽셀 폭을 유지하고 Minor Grid의 계단 현상을 줄인다.
    float2 MinorCoordinates = WorldPosition.xy / GridSpacing;
    float2 MinorDerivatives = max(fwidth(MinorCoordinates), 0.00001f);
    float2 MinorDistance = abs(frac(MinorCoordinates - 0.5f) - 0.5f) / MinorDerivatives;
    float MinorAlpha = saturate(GridLineWidth - min(MinorDistance.x, MinorDistance.y));

    // 일정 간격마다 더 밝은 Major Grid를 같은 방식으로 계산한다.
    float2 MajorCoordinates = WorldPosition.xy / MajorGridSpacing;
    float2 MajorDerivatives = max(fwidth(MajorCoordinates), 0.00001f);
    float2 MajorDistance = abs(frac(MajorCoordinates - 0.5f) - 0.5f) / MajorDerivatives;
    float MajorAlpha = saturate(GridLineWidth - min(MajorDistance.x, MajorDistance.y));

    // Far Plane에서 갑자기 잘리지 않도록 View와의 3차원 거리로 먼저 페이드한다.
    float ViewDistance = length(WorldPosition - ViewOrigin);
    float FadeStart = min(FadeDistance * 0.5f, FarClip * 0.8f);
    float FadeEnd = min(FadeDistance, FarClip * 0.95f);
    float Fade = 1.0f - smoothstep(FadeStart, FadeEnd, ViewDistance);
    float4 GridColor = lerp(MinorColor, MajorColor, MajorAlpha);
    GridColor.a *= max(MinorAlpha, MajorAlpha) * Fade;
    if (GridColor.a <= 0.0f)
    {
        discard;
    }

    // 교차점이 현재 View의 Clip 범위에 있을 때만 실제 깊이를 기록한다.
    float4 ClipPosition = mul(float4(WorldPosition, 1.0f), ViewProjection);
    if (ClipPosition.w <= 0.0f)
    {
        discard;
    }

    float Depth = ClipPosition.z / ClipPosition.w;
    if (Depth < 0.0f || Depth > 1.0f)
    {
        discard;
    }
    // 선택 물체의 2 Pixel Outline 위치에서 물체보다 뒤에 있는 Grid만 버린다.
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
            if (OutlineDepth > Depth)
            {
                discard;
            }
        }
    }

    PS_OUTPUT Output;
    Output.Color = GridColor;
    Output.Depth = Depth;
    return Output;
}
