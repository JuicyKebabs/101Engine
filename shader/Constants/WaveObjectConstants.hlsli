// Constant-buffer for wave object constants
cbuffer WaveObjectConstants : register(b1)
{
    float4x4 world;         // World matrix
    
    float2 waveDirection;   // Wave direction vector
    float waveAmplitude;    // Wave amplitude
    float waveFrequency;    // Wave frequency
    
    float time;             // Time parameter for animating the wave
    uint subdivisionX;      // Number of subdivisions in the X direction
    uint subdivisionY;      // Number of subdivisions in the Y direction
    uint padding;           // Padding to align to 16-byte boundary
    
    float4 color;           // Overall color for the wave object
    
    float4x4 worldInvTranspose; // Inverse transpose of the world matrix
}