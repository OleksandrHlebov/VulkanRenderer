#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;


layout (location = 0) out vec2 outUV;

layout (set = 1, binding = 0) uniform ModelViewProjection
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} mvp;

void main()
{
    gl_Position = mvp.Projection * mvp.View * mvp.Model * vec4(inPosition, 1.);
    outUV = inUV;
}