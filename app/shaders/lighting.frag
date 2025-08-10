#version 450

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 1, binding = 1) uniform texture2D albedo;

void main()
{
    outColour = texture(sampler2D(albedo, samp), inUV);
}