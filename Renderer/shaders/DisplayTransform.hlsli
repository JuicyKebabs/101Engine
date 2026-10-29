// Both functions receive exposed nonnegative linear RGB. They return compressed
// linear RGB; neither applies exposure, sRGB encoding nor Alpha conversion.
float3 ReinhardPerChannel(float3 exposedLinearRGB)
{
    float3 color = max(exposedLinearRGB, 0);
    // Equivalent to c/(1+c), with a bounded numerator for very bright FP16 input.
    return 1 - rcp(1 + color);
}

float3 ReinhardLuminance(float3 exposedLinearRGB)
{
    float3 color = max(exposedLinearRGB, 0);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    // The common scale preserves RGB ratios. Individual channels can exceed 1;
    // the target format's representable range remains a separate limitation.
    return color / (1 + luminance);
}
