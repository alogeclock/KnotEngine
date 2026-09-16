// 정점 버퍼 없이 World 원점부터 양의 X/Y/Z 방향으로 뻗는 축을 그린다.
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Color : COLOR;
};

// b0은 View마다 갱신하는 공용 상수 슬롯이다.
cbuffer ViewConstants : register(b0)
{
    row_major float4x4 ViewProjection;
    row_major float4x4 InverseViewProjection;
    float3 ViewOrigin;
    float FarClip;
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
    float DistanceFromView = length(Input.WorldPosition - ViewOrigin);
    float Alpha = 1.0f - smoothstep(FadeStart, FadeEnd, DistanceFromView);
    if (Alpha <= 0.0f)
    {
        discard;
    }

    return float4(Input.Color, Alpha);
}
