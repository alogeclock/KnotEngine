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

// b0은 View마다 갱신하는 공용 상수 슬롯이다.
cbuffer ViewConstants : register(b0)
{
    row_major float4x4 ViewProjection;
    row_major float4x4 InverseViewProjection;
    float3 ViewOrigin;
    float ViewPadding;
};

// b1은 Pass마다 한 번 갱신하는 상수 슬롯이다.
cbuffer GridConstants : register(b1)
{
    float FadeDistance;
    float GridSpacing;
    float MajorGridInterval;
    float LineWidth;

    float4 MinorColor;
    float4 MajorColor;
};

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
    // NDC의 Near/Far 지점을 World 공간으로 역투영하여 픽셀별 광선을 만든다.
    float4 NearPosition = mul(float4(Input.NdcPosition, 0.0f, 1.0f), InverseViewProjection);
    float4 FarPosition = mul(float4(Input.NdcPosition, 1.0f, 1.0f), InverseViewProjection);
    NearPosition /= NearPosition.w;
    FarPosition /= FarPosition.w;

    // World Z=0 평면과 평행하거나 View 앞에서 평면과 만나지 않는 광선은 버린다.
    float3 RayDirection = FarPosition.xyz - NearPosition.xyz;
    if (abs(RayDirection.z) < 0.00001f)
    {
        discard;
    }

    float RayDistance = -NearPosition.z / RayDirection.z;
    if (RayDistance < 0.0f || RayDistance > 1.0f)
    {
        discard;
    }

    float3 WorldPosition = NearPosition.xyz + RayDirection * RayDistance;
    float MajorGridSpacing = GridSpacing * MajorGridInterval;

    // 화면 미분값으로 선의 픽셀 폭을 유지하고 Minor Grid의 계단 현상을 줄인다.
    float2 MinorCoordinates = WorldPosition.xy / GridSpacing;
    float2 MinorDerivatives = max(fwidth(MinorCoordinates), 0.00001f);
    float2 MinorDistance = abs(frac(MinorCoordinates - 0.5f) - 0.5f) / MinorDerivatives;
    float MinorAlpha = saturate(LineWidth - min(MinorDistance.x, MinorDistance.y));

    // 일정 간격마다 더 밝은 Major Grid를 같은 방식으로 계산한다.
    float2 MajorCoordinates = WorldPosition.xy / MajorGridSpacing;
    float2 MajorDerivatives = max(fwidth(MajorCoordinates), 0.00001f);
    float2 MajorDistance = abs(frac(MajorCoordinates - 0.5f) - 0.5f) / MajorDerivatives;
    float MajorAlpha = saturate(LineWidth - min(MajorDistance.x, MajorDistance.y));

    // View에서 멀어질수록 Grid를 페이드하고 두 Grid 단계의 색상과 Alpha를 합성한다.
    float DistanceFromView = length(WorldPosition.xy - ViewOrigin.xy);
    float Fade = 1.0f - smoothstep(FadeDistance * 0.5f, FadeDistance, DistanceFromView);
    float4 GridColor = lerp(MinorColor, MajorColor, MajorAlpha);
    GridColor.a *= max(MinorAlpha, MajorAlpha) * Fade;
    if (GridColor.a <= 0.0f)
    {
        discard;
    }

    // World 평면의 실제 깊이를 기록하여 Scene Geometry와 정상적으로 깊이 테스트한다.
    float4 ClipPosition = mul(float4(WorldPosition, 1.0f), ViewProjection);
    PS_OUTPUT Output;
    Output.Color = GridColor;
    Output.Depth = ClipPosition.z / ClipPosition.w;
    return Output;
}
