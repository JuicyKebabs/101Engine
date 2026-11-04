Texture2D SelectionMask : register(t0);

struct Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(Input input) : SV_TARGET
{
    uint width;
    uint height;
    
    SelectionMask.GetDimensions(width, height);
    
    // Get the pixel coordinates of the current fragment.
    int2 pixel = int2(input.position.xy);
    
    // Read the alpha value of the current pixel from the selection mask.
    float center = SelectionMask.Load(int3(pixel, 0)).a;
    
    float nearby = 0;

    // Match a two-pixel outline while reading only numeric mask Alpha. An
    // emergency Geometry texture's RGB checker cannot introduce mask holes.
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 samplePixel = clamp(pixel + int2(x, y), int2(0, 0), int2(width - 1, height - 1));
            nearby = max(nearby, SelectionMask.Load(int3(samplePixel, 0)).a);
        }
    }

    return float4(1.0f, 0.3185468f, 0.0331048f, saturate(nearby - center));
}