#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout (location = 0) in vec2 inUV;
layout (location = 1) in mat3 inTBN;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (push_constant) uniform constants
{
    uint Diffuse;
    uint Normals;
    uint Metalness;
    uint Roughness;
} textureIndices;

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
vec2 OctWrap(vec2 v)
{
    return (1.0 - abs(v.yx)) * vec2(v.x >= 0.0 ? 1.0 : -1.0,
    v.y >= 0.0 ? 1.0 : -1.0);
}

vec2 Encode(vec3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + vec2(0.5, 0.5);
    return n.xy;
}

layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outMaterial;

void main()
{
    vec3 normal = texture(sampler2D(textures[nonuniformEXT(textureIndices.Normals)], samp), inUV).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(inTBN * normal);

    const float metalness = texture(sampler2D(textures[nonuniformEXT(textureIndices.Metalness)], samp), inUV).b;
    const float roughness = texture(sampler2D(textures[nonuniformEXT(textureIndices.Roughness)], samp), inUV).g;

    outAlbedo = texture(sampler2D(textures[nonuniformEXT(textureIndices.Diffuse)], samp), inUV);
    outMaterial = vec4(Encode(normal).rg, roughness, metalness);
}