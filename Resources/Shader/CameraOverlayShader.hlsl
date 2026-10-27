cbuffer CameraOverlayConstants : register(b0)
{
    float4 FadeColor;
    float LetterBoxHeight;
    float3 Padding;
};

struct FPSInput
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    Output.UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(Output.UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    // 검은 띠 위에도 Fade를 합성해 완전한 색상 Fade가 화면 전체를 덮도록 한다.
    if (Input.UV.y < LetterBoxHeight || Input.UV.y >= 1.0f - LetterBoxHeight)
    {
        return float4(FadeColor.rgb * FadeColor.a, 1.0f);
    }
    return FadeColor;
}
