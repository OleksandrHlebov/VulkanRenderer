#version 450
#extension GL_EXT_nonuniform_qualifier: require

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec4 inPosition;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 0, binding = 1) uniform texture2D textures[];

layout (push_constant) uniform constants
{
    vec3 LightPosition;
    float FarPlane;
    layout(offset = 80)uint Diffuse;
};

void main()
{
    const float alphaThreshold = .95f;
    if (texture(sampler2D(textures[nonuniformEXT(Diffuse)], samp), inUV).a < alphaThreshold)
    discard;

    // get distance between fragment and light source
    float lightDistance = length(inPosition.xyz - LightPosition);

    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / FarPlane;

    // write this as modified depth
    gl_FragDepth = lightDistance;
}