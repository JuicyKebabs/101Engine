cbuffer LightConstants : register(b2)
{
    float4 lightDir_Intensity; // Light direction (x, y, z) and intensity (w)
    float4 lightColor_Ambient; // Light color (x, y, z) and ambient light intensity (w)
};