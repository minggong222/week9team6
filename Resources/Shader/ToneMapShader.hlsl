Texture2D<float4> LightingHDR : register(t0);
cbuffer ToneMapConstants : register(b0)
{
    float ColorGradingGamma;
    uint bEnableGammaCorrection;
    float2 Padding;
};

struct FPSInput
{
    float4 Position : SV_POSITION;
};

FPSInput mainVS(uint VertexID : SV_VertexID)
{
    FPSInput Output;
    float2 UV = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(UV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}

float4 mainPS(FPSInput Input) : SV_Target
{
    int2 Pixel = int2(Input.Position.xy);
    float3 HDR = max(LightingHDR.Load(int3(Pixel, 0)).rgb, 0.0f);
    
    // 밝은 부분을 부드럽게 압축하는 ACES 근사 톤 매핑.
    float3 Mapped = (HDR * (2.51f * HDR + 0.03f)) / (HDR * (2.43f * HDR + 0.59f) + 0.14f);
    
    float3 Graded = pow(saturate(Mapped), 1.0f / max(ColorGradingGamma, 0.01f));

    float3 DisplayColor = Graded;
    if (bEnableGammaCorrection != 0)
        DisplayColor = pow(Graded, 1.0f / 2.2f);

    return float4(DisplayColor, 1.0f);
}
