// Per-frame constants for the shader
cbuffer FrameConstants : register(b0)
{
    float4x4 view;    // View matrix
    float4x4 proj;    // Projection matrix
    float3 cameraPos; // Camera position in world space
};