#version 450

layout (location = 0) out vec3 outColor;

layout (binding = 0) uniform ModelViewProjection
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
} mvp;

vec2 positions[3] = vec2[](vec2(0.0, 0.5), vec2(-0.5, -0.5), vec2(0.5, -0.5));
vec3 colors[3] = vec3[](vec3(1., .0, .0), vec3(.0, 1., .0), vec3(.0, .0, 1.));

void main()
{
    gl_Position = mvp.Projection * mvp.View * mvp.Model * vec4(positions[gl_VertexIndex], 1., 1.);
    outColor = colors[gl_VertexIndex];
}