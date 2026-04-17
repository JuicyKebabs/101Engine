// Vertex output structure
struct VSOutPut
{
    float4 svpos : SV_POSITION; // Vertex position after transformation
    float4 color : COLOR;       // Vertex color
    float2 uv : TEXCOORD;       // Texture coordinates
    float3 normal : NORMAL;     // Normal vector
};