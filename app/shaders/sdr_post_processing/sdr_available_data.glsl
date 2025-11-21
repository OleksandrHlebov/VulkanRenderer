// this header contains all the data at your disposal from the renderer
// to use in the custom sdr post-processing shader
// it has to be included in .frag for compilation using:
//#extension GL_GOOGLE_include_directive: require
//#include "sdr_available_data.glsl"
#extension GL_EXT_samplerless_texture_functions: require

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

layout (set = 0, binding = 0) uniform sampler samp;

layout (set = 1, binding = 0) uniform texture2D albedo;
layout (set = 1, binding = 1) uniform texture2D material;
layout (set = 1, binding = 2) uniform texture2D depthBuffer;
layout (set = 1, binding = 4) uniform texture2D SDRImage[2];