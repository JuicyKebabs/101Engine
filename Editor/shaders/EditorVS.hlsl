struct Input
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct Output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Output main(Input input)
{
    Output output;
    output.position = float4(input.position, 1);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}