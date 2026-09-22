struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR;
	float Visibility : TEXCOORD0;
};

cbuffer ViewConstants : register(b0)
{
	row_major float4x4 ViewProjection;
};

cbuffer GizmoConstants : register(b1)
{
	float3 Origin;
	float WorldScale;

	float3 ViewRight;
	uint Mode;

	float3 ViewUp;
	int HighlightedAxis;

	float3 ViewOrigin;
	uint Padding;

	float2 ViewportSize;
	float2 Padding2;
};

static const float Tau = 6.28318530718f;
static const float LineHalfWidthPixels = 2.0f;
static const float3 AxisColors[3] = { float3(0.86f, 0.18f, 0.18f), float3(0.25f, 0.72f, 0.30f), float3(0.20f, 0.42f, 0.92f), };

// Axis Index를 X, Y, Z 단위 축 벡터로 변환한다.
float3 GetAxis(uint AxisIndex)
{
	return AxisIndex == 0 ? float3(1.0f, 0.0f, 0.0f) : AxisIndex == 1 ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
}

// 기준 축에 수직인 두 Basis 축 중 하나를 반환한다.
float3 GetSide(uint AxisIndex, uint SideIndex)
{
	if (AxisIndex == 0) return SideIndex == 0 ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	if (AxisIndex == 1) return SideIndex == 0 ? float3(1.0f, 0.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
	return SideIndex == 0 ? float3(1.0f, 0.0f, 0.0f) : float3(0.0f, 1.0f, 0.0f);
}

// 로컬 좌표를 월드-뷰-투영 변환하여 클립 좌표로 이동시킨다.
float4 Transform(float3 LocalPosition)
{
	return mul(float4(Origin + LocalPosition * WorldScale, 1.0f), ViewProjection);
}

// 클립 공간 선분을 화면에서 일정한 픽셀 두께를 가진 Quad 정점으로 확장한다.
float4 ExpandScreenLine(float4 StartClip, float4 EndClip, uint Corner, float HalfWidthPixels)
{
	const float2 StartNdc = StartClip.xy / StartClip.w;
	const float2 EndNdc = EndClip.xy / EndClip.w;
	const float2 PixelDirection = (EndNdc - StartNdc) * ViewportSize;
	const float PixelLength = max(length(PixelDirection), 0.0001f);
	const float2 Perpendicular = float2(-PixelDirection.y, PixelDirection.x) / PixelLength;
	const bool UseEnd = Corner == 1 || Corner == 2 || Corner == 4;
	const bool UsePositiveSide = Corner == 2 || Corner == 4 || Corner == 5;
	float4 Result = UseEnd ? EndClip : StartClip;
	const float Side = UsePositiveSide ? 1.0f : -1.0f;
	Result.xy += Perpendicular * Side * HalfWidthPixels * 2.0f / ViewportSize * Result.w;
	return Result;
}

// 월드 선분을 화면 공간에서 일정한 픽셀 두께를 가진 Quad 정점으로 확장한다.
float4 BuildScreenLineClip(float3 Start, float3 End, uint Corner, float HalfWidthPixels)
{
	return ExpandScreenLine(Transform(Start), Transform(End), Corner, HalfWidthPixels);
}

// 3D 회전 구의 점을 Gizmo 중심 깊이의 화면 평면에 직교 투영한다.
float4 ProjectRotationPoint(float3 LocalPosition)
{
	const float4 CenterClip = Transform(float3(0.0f, 0.0f, 0.0f));
	const float4 RightClip = Transform(ViewRight);
	const float4 UpClip = Transform(ViewUp);
	const float2 CenterNdc = CenterClip.xy / CenterClip.w;
	const float2 RightNdc = RightClip.xy / RightClip.w - CenterNdc;
	const float2 UpNdc = UpClip.xy / UpClip.w - CenterNdc;
	const float2 OffsetNdc = RightNdc * dot(LocalPosition, ViewRight) + UpNdc * dot(LocalPosition, ViewUp);
	float4 Result = CenterClip;
	Result.xy += OffsetNdc * Result.w;
	return Result;
}

// 회전 구의 선분을 흰색 View Ring 안에 유지되는 화면 공간 선으로 생성한다.
float4 BuildRotationLineClip(float3 Start, float3 End, uint Corner, float HalfWidthPixels)
{
	return ExpandScreenLine(ProjectRotationPoint(Start), ProjectRotationPoint(End), Corner, HalfWidthPixels);
}

// 이동 축 끝에 표시할 원뿔형 화살촉의 로컬 정점을 생성한다.
float3 BuildArrowHead(uint AxisIndex, uint VertexId)
{
	const float3 Axis = GetAxis(AxisIndex);
	const float3 Side0 = GetSide(AxisIndex, 0);
	const float3 Side1 = GetSide(AxisIndex, 1);
	const uint Segment = VertexId / 3;
	const uint Corner = VertexId % 3;
	const float Angle = Tau * (float)(Segment + (Corner == 2 ? 1 : 0)) / 6.0f;
	return Corner == 0 ? Axis : Axis * 0.74f + (Side0 * cos(Angle) + Side1 * sin(Angle)) * 0.105f;
}

// 스케일 축 끝에 표시할 큐브의 로컬 정점을 생성한다.
float3 BuildArrowCube(uint AxisIndex, uint VertexId)
{
	const float3 Basis[3] = { GetAxis(AxisIndex), GetSide(AxisIndex, 0), GetSide(AxisIndex, 1) };
	const uint Face = VertexId / 6;
	const uint Corner = VertexId % 6;
	const uint Dimension = Face / 2;
	const float Sign = (Face & 1) == 0 ? -1.0f : 1.0f;
	const float2 Corners[6] =
	{
		float2(-1.0f, -1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f),
		float2(-1.0f, -1.0f), float2(1.0f, 1.0f), float2(-1.0f, 1.0f),
	};
	const uint Dimension1 = (Dimension + 1) % 3;
	const uint Dimension2 = (Dimension + 2) % 3;
	return GetAxis(AxisIndex) + (Basis[Dimension] * Sign + Basis[Dimension1] * Corners[Corner].x + Basis[Dimension2] * Corners[Corner].y) * 0.075f;
}

// 지정한 축에 수직인 회전 원에서 현재 선분의 시작점과 끝점을 계산한다.
void GetRotationSegment(uint AxisIndex, uint VertexId, out float3 Start, out float3 End)
{
	const float3 Side0 = GetSide(AxisIndex, 0);
	const float3 Side1 = GetSide(AxisIndex, 1);
	const uint Segment = VertexId / 6;
	const float Angle0 = Tau * (float)Segment / 64.0f;
	const float Angle1 = Tau * (float)(Segment + 1) / 64.0f;
	Start = Side0 * cos(Angle0) + Side1 * sin(Angle0);
	End = Side0 * cos(Angle1) + Side1 * sin(Angle1);
}

// 카메라를 향하는 원을 화면 공간에서 일정한 픽셀 두께의 선분으로 생성한다.
float4 BuildViewRingClip(uint VertexId, float Radius, float HalfWidthPixels)
{
	const uint Segment = VertexId / 6;
	const uint Corner = VertexId % 6;
	const float Angle0 = Tau * (float)Segment / 64.0f;
	const float Angle1 = Tau * (float)(Segment + 1) / 64.0f;
	const float3 Start = (ViewRight * cos(Angle0) + ViewUp * sin(Angle0)) * Radius;
	const float3 End = (ViewRight * cos(Angle1) + ViewUp * sin(Angle1)) * Radius;
	return BuildScreenLineClip(Start, End, Corner, HalfWidthPixels);
}

// 카메라를 향하는 중앙 원형 Handle의 삼각형 정점을 생성한다.
float3 BuildCenterVertex(uint VertexId)
{
	const uint Segment = VertexId / 3;
	const uint Corner = VertexId % 3;
	if (Corner == 0) return float3(0.0f, 0.0f, 0.0f);
	const float Angle = Tau * (float)(Segment + (Corner == 2 ? 1 : 0)) / 8.0f;
	return (ViewRight * cos(Angle) + ViewUp * sin(Angle)) * 0.035f;
}

// Trackball Hover 표시에 사용하는 카메라 정렬 원판의 삼각형 정점을 생성한다.
float3 BuildTrackballDisc(uint VertexId)
{
	const uint Segment = VertexId / 3;
	const uint Corner = VertexId % 3;
	if (Corner == 0) return float3(0.0f, 0.0f, 0.0f);
	const float Angle = Tau * (float)(Segment + (Corner == 2 ? 1 : 0)) / 64.0f;
	return ViewRight * cos(Angle) + ViewUp * sin(Angle);
}

// 축의 기본 색상과 Hover 또는 드래그 강조 색상을 반환한다.
float4 GetAxisColor(uint AxisIndex)
{
	const bool IsHighlighted = (int)AxisIndex == HighlightedAxis;
	return float4(IsHighlighted ? float3(1.0f, 0.72f, 0.12f) : AxisColors[AxisIndex], IsHighlighted ? 1.0f : 0.6f);
}

VS_OUTPUT GizmoVS(uint VertexId : SV_VertexID)
{
	VS_OUTPUT Output;
	Output.Visibility = 1.0f;

	if (Mode == 1) // Rotation 기즈모
	{
		if (VertexId < 192) // Trackball Hover 원판의 정점 범위
		{
			Output.Position = Transform(BuildTrackballDisc(VertexId));
			Output.Color = float4(0.82f, 0.82f, 0.82f, 0.18f);
			Output.Visibility = HighlightedAxis == 4 ? 1.0f : -1.0f;
		}
		else if (VertexId < 1344) // X, Y, Z 회전 링의 정점 범위
		{
			const uint AxisVertex = VertexId - 192;
			const uint AxisIndex = AxisVertex / 384;
			const uint LocalVertex = AxisVertex % 384;
			float3 Start;
			float3 End;
			GetRotationSegment(AxisIndex, LocalVertex, Start, End);
			Output.Position = BuildRotationLineClip(Start, End, LocalVertex % 6, LineHalfWidthPixels);
			Output.Color = GetAxisColor(AxisIndex);
			Output.Visibility = dot((Start + End) * 0.5f, ViewOrigin - Origin);
		}
		else if (VertexId < 1728) // 카메라를 향하는 흰색 외곽 링의 정점 범위
		{
			Output.Position = BuildViewRingClip(VertexId - 1344, 1.1325f, 1.5f);
			const bool IsHighlighted = HighlightedAxis == 3;
			Output.Color = float4(IsHighlighted ? float3(1.0f, 0.72f, 0.12f) : float3(0.82f, 0.82f, 0.82f), IsHighlighted ? 1.0f : 0.6f);
		}
		else // 중앙의 노란 점
		{
			Output.Position = Transform(BuildCenterVertex(VertexId - 1728));
			Output.Color = float4(1.0f, 0.52f, 0.08f, HighlightedAxis == 4 ? 1.0f : 0.8f);
		}
		return Output;
	}

	// Translation 기즈모와 Scale 기즈모
	const uint VerticesPerAxis = Mode == 0 ? 24 : 42;
	const uint AxisVertexCount = VerticesPerAxis * 3;
	if (VertexId < AxisVertexCount)
	{
		const uint AxisIndex = VertexId / VerticesPerAxis;
		const uint LocalVertex = VertexId % VerticesPerAxis;
		if (LocalVertex < 6)
		{
			const float End = Mode == 0 ? 0.76f : 0.925f;
			Output.Position = BuildScreenLineClip(GetAxis(AxisIndex) * 0.16f, GetAxis(AxisIndex) * End, LocalVertex, LineHalfWidthPixels);
		}
		else
		{
			const float3 LocalPosition = Mode == 0 ? BuildArrowHead(AxisIndex, LocalVertex - 6) : BuildArrowCube(AxisIndex, LocalVertex - 6);
			Output.Position = Transform(LocalPosition);
		}
		Output.Color = GetAxisColor(AxisIndex);
		return Output;
	}

	const uint CenterLocalVertex = VertexId - AxisVertexCount;
	if (CenterLocalVertex < 384)
	{
		Output.Position = BuildViewRingClip(CenterLocalVertex, 0.17f, 1.5f);
		const bool IsHighlighted = HighlightedAxis == 5;
		Output.Color = float4(IsHighlighted ? float3(1.0f, 0.72f, 0.12f) : float3(0.82f, 0.82f, 0.82f), IsHighlighted ? 1.0f : 0.6f);
	}
	else
	{
		Output.Position = Transform(BuildCenterVertex(CenterLocalVertex - 384));
		Output.Color = float4(1.0f, 0.52f, 0.08f, HighlightedAxis == 5 ? 1.0f : 0.8f);
	}
	return Output;
}

float4 GizmoPS(VS_OUTPUT Input) : SV_TARGET
{
	clip(Input.Visibility);
	return Input.Color;
}
