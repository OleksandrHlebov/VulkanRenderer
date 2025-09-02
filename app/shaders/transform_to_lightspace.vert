#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (location = 0) out vec2 outUV;
layout (push_constant) uniform Constants
{
    layout (offset = 16)mat4 lightSpaceTransform;
};

void main()
{
    gl_Position = lightSpaceTransform * vec4(inPosition, 1.f);
    outUV = inUV;
}