#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : require

layout(location = 0) out vec4 outColour;
layout(location = 0) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler samp;

layout(set = 1, binding = 3) uniform texture2D HDRImage[2];

layout(push_constant) uniform Constants
{
    uint imageIndex;
};

float CalculateEV100FromPhysicalCamera(in float aperture, in float shutterTime, in float ISO)
{
    return log2(pow(aperture, 2) / shutterTime * 100 / ISO);
}

float CalculateEV100FromAverageLuminance(in float averageLuminance)
{
    const float K = 12.5f;
    return log2((averageLuminance * 100.f) / K);
}

float ConvertEV100ToExposure(in float EV100)
{
    const float maxLuminance = 1.2f * pow(2.f, EV100);
    return 1.f / max(maxLuminance, 0.0001f);
}

vec3 Uncharted2ToneMappingCurve(in vec3 color)
{
    const float a = .15f;
    const float b = .5f;
    const float c = .1f;
    const float d = .2f;
    const float e = .02f;
    const float f = .3f;
    return ((color * (a * color + c * b) + d * e)
    / (color * (a * color + b) + d * f))
    - e / f;
}

vec3 Uncharted2ToneMapping(in vec3 color)
{
    const float W = 11.2f;
    const vec3 curvedColor = Uncharted2ToneMappingCurve(color);
    const float whiteScale = 1.f / Uncharted2ToneMappingCurve(vec3(W)).r;
    return clamp(curvedColor * whiteScale, .0f, 1.f);
}

void main()
{
    float currentEV = 1.f;
    #ifdef SUNNY_16
    const float aperture = 5.f;
    const float ISO = 100.f;
    const float shutterSpeed = 1.f / 200.f;
    currentEV = CalculateEV100FromPhysicalCamera(aperture, shutterSpeed, ISO);
    #else
    #ifdef INDOOR
    const float aperture = 1.6f;
    const float ISO = 1600.f;
    const float shutterSpeed = 1.f / 60.f;
    currentEV = CalculateEV100FromPhysicalCamera(aperture, shutterSpeed, ISO);
    #endif
    #endif

    const float exposure = ConvertEV100ToExposure(currentEV);
    const vec3 hdrColor = texelFetch(sampler2D(HDRImage[imageIndex], samp), ivec2(inUV.xy * textureSize(HDRImage[imageIndex], 0)), 0).rgb;

    outColour = vec4(Uncharted2ToneMapping(hdrColor * exposure), 1.f);
}