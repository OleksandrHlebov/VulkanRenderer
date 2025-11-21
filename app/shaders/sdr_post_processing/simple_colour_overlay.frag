#version 450
#extension GL_ARB_gpu_shader_int64: require
#extension GL_GOOGLE_include_directive: require
#include "sdr_available_data.glsl"

layout (push_constant) uniform constants
{
    uint64_t time;
    uint lastImageIndex;
    vec3 colour;
};

void main()
{
    vec4 original = texelFetch(sampler2D(SDRImage[lastImageIndex], samp), ivec2(inUV.xy * textureSize(SDRImage[lastImageIndex], 0)), 0);

    outColour = vec4(clamp(original.rgb + colour, .0f, 1.f), 1.f);
}