#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout (location = 0) in vec2 inUV;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (push_constant) uniform constants
{
    uint Diffuse;
} textureIndices;

void main()
{
    const float alphaThreshold = .95f;
    if (texture(sampler2D(textures[nonuniformEXT(textureIndices.Diffuse)], samp), inUV).a < alphaThreshold)
    discard;
}