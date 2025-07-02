#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout (location = 0) in vec2 inUV;

layout (constant_id = 0) const uint TEXTURE_COUNT = 1;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 0, binding = 1) uniform texture2D textures[TEXTURE_COUNT];

layout (push_constant) uniform constants
{
    uint Diffuse;
} textureIndices;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sampler2D(textures[nonuniformEXT(textureIndices.Diffuse)], samp), inUV);
}