Texture2D Image : register(t0);
SamplerState LinearClamp : register(s1);

struct Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(Input input) : SV_TARGET
{
    return Image.Sample(LinearClamp, input.uv) * input.color;
}