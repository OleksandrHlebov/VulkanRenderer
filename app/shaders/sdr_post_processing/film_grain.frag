#version 450
#extension GL_ARB_gpu_shader_int64: require
#extension GL_ARB_shading_language_include: require
#include "sdr_available_data.glsl"
//https://godotshaders.com/shader/film-grain-shader/
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

layout (push_constant) uniform constants
{
    uint64_t time;
    uint lastImageIndex;
    float grainAmount;
    float grainSize;
    vec2 frequency;
    vec3 colour;
    uint seed;
    bool b_Animate;
};

void main()
{
    //    pseudoRNGState = WangHash(uvec3(time & float(bAnimate), gl_FragCoord.x, gl_FragCoord.y));
    pseudoRNGState = WangHash(uvec3(time * uint(b_Animate) + seed * (1 - uint(b_Animate)), gl_FragCoord.x * frequency.x, gl_FragCoord.y * frequency.y));
    vec4 original = texelFetch(sampler2D(SDRImage[lastImageIndex], samp), ivec2(inUV.xy * textureSize(SDRImage[lastImageIndex], 0)), 0);

    float noise = float(Rand()) * (1.0 / 4294967296.0);
    original.rgb += colour * noise * grainAmount * grainSize;

    outColour = clamp(original, .0, 1.);
}