#version 450

layout (set = 1, binding = 0) uniform ModelViewProjection
{
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;

layout (location = 0) out vec2 outUV;
layout (location = 1) out mat3 outTBN;

void main()
{
    const vec3 T = normalize(vec3(mvp.model * vec4(tangent, 0.0)));
    const vec3 B = normalize(vec3(mvp.model * vec4(bitangent, 0.0)));
    const vec3 N = normalize(vec3(mvp.model * vec4(normal, 0.0)));
    outTBN = mat3(T, B, N);

    gl_Position = mvp.projection * mvp.view * mvp.model * vec4(inPosition, 1.);
    outUV = inUV;
}
