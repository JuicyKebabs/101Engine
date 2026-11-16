StructuredBuffer<float4> Positions : register(t0);

float4 main(uint vertexId : SV_VertexID) : SV_POSITION
{
    return Positions[vertexId];
}
