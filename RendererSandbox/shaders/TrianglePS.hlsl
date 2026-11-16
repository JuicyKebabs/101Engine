Texture2D<float4> DemoTexture : register(t1);
StructuredBuffer<float4> Color : register(t3);

float4 main() : SV_TARGET
{
    return Color[0] * DemoTexture.Load(int3(0, 0, 0));
}
