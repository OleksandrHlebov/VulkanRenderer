#version 450
#extension GL_EXT_samplerless_texture_functions: require
#extension GL_ARB_gpu_shader_int64: require
//https://godotshaders.com/shader/film-grain-shader/
layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

float grainAmount = .05f;
float grainSize = .5f;

// references used for randomness
// https://amindforeverprogramming.blogspot.com/2013/07/random-floats-in-glsl-330.html
// https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint WangHash(uvec2 seed)
{
    return WangHash(seed.x ^ WangHash(seed.y));
}

uint WangHash(uvec3 seed)
{
    return WangHash(seed.x ^ WangHash(seed.yz));
}

uint pseudoRNGState;

uint Rand()
{
    // Xorshift algorithm from George Marsaglia's paper
    pseudoRNGState ^= (pseudoRNGState << 13);
    pseudoRNGState ^= (pseudoRNGState >> 17);
    pseudoRNGState ^= (pseudoRNGState << 5);
    return pseudoRNGState;
}

layout (set = 0, binding = 0) uniform sampler samp;

layout (set = 1, binding = 4) uniform texture2D SDRImage[2];

layout (push_constant) uniform constants
{
    uint64_t time;
    uint lastImageIndex;
};

void main()
{
    pseudoRNGState = WangHash(uvec3(time, gl_FragCoord.x, gl_FragCoord.y));
    vec4 original = texelFetch(sampler2D(SDRImage[lastImageIndex], samp), ivec2(inUV.xy * textureSize(SDRImage[lastImageIndex], 0)), 0);

    vec2 randomVector = vec2(
    float(Rand()) * (1.0 / 4294967296.0),
    float(Rand()) * (1.0 / 4294967296.0)
    );

    float noise = (fract(sin(dot(randomVector, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * 2.0;
    original.rgb += noise * grainAmount * grainSize;

    outColour = clamp(original, .0, 1.);
}