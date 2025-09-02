#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions: require

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 1, binding = 1) uniform texture2D albedo;
layout (set = 1, binding = 2) uniform texture2D material;
layout (set = 1, binding = 3) uniform texture2D depthBuffer;
layout (set = 1, binding = 0) uniform ModelViewProjection
{
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp;

float PI = 3.14159265358979323846;

struct Light
{
    vec4 position;
    vec4 colour;
    uint matrixIndex;
};

layout(constant_id = 0) const uint LIGHT_COUNT = 1u;
layout(constant_id = 1) const uint DIRECTIONAL_LIGHT_COUNT = 1u;
layout(std430, set = 1, binding = 4) readonly buffer LightsSSBO
{
    Light lights[LIGHT_COUNT];
};
layout(std430, set = 1, binding = 5) readonly buffer LightMatricesSSBO
{
    mat4 matrices[DIRECTIONAL_LIGHT_COUNT];
};
layout(set = 1, binding = 6) uniform sampler shadowSampler;
layout(set = 1, binding = 7) uniform texture2D directionalShadowMaps[DIRECTIONAL_LIGHT_COUNT];

float DistributionGGX(vec3 N, vec3 H, float a)
{
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
vec3 WorldPosFromDepth(float depth, vec2 texCoord) {
    vec4 clipSpacePosition = vec4(texCoord * 2.0 - vec2(1.0, 1.0), depth, 1.0);
    vec4 viewSpacePosition = inverse(mvp.projection) * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = inverse(mvp.view) * viewSpacePosition;

    return worldSpacePosition.xyz;
}

vec3 Decode(vec2 f)
{
    f = f * 2.0 - vec2(1.0, 1.0);

    // https://twitter.com/Stubbesaurus/status/937994790553227264
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += vec2((n.x >= 0.0) ? -t : t,
    (n.y >= 0.0) ? -t : t);
    return normalize(n);
}

void main()
{
    const vec4 materialProperties = texelFetch(sampler2D(material, samp), ivec2(inUV * textureSize(material, 0)), 0);
    const vec4 albedoColour = texture(sampler2D(albedo, samp), inUV);
    const vec3 normal = Decode(materialProperties.rg);

    const float metalness = materialProperties.a;
    const float roughness = materialProperties.b;

    const float depth = texelFetch(sampler2D(depthBuffer, samp), ivec2(inUV * textureSize(depthBuffer, 0)), 0).r;
    const vec3 worldPosition = WorldPosFromDepth(depth, inUV);
    const vec3 cameraPosition = inverse(mvp.view)[3].xyz;
    const vec3 viewDirection = normalize(cameraPosition - worldPosition);

    vec3 Lo = vec3(0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedoColour.rgb, metalness);
    for (uint lightIndex = 0u; lightIndex < LIGHT_COUNT; ++lightIndex)
    {

        const float isPoint = lights[lightIndex].position.w;
        const float isDirectional = 1.f - isPoint;

        const vec3 lightDirection = normalize(isPoint * (lights[lightIndex].position.xyz - worldPosition)
        + isDirectional * lights[lightIndex].position.xyz);

        const vec3 halfway = normalize(viewDirection + lightDirection);

        const float lumen = lights[lightIndex].colour.a;
        const float luminousIntensity = lumen / (4.f * PI);
        const float distance = length(lights[lightIndex].position.xyz - worldPosition);
        const float attenuation = 1.f / max(distance * distance, 0.0001f);

        const float illuminance = isPoint * luminousIntensity * attenuation
        + isDirectional * lights[lightIndex].colour.a;
        const vec3 irradiance = lights[lightIndex].colour.rgb * illuminance;
        const vec3 F = FresnelSchlick(max(dot(halfway, viewDirection), .0f), F0);
        const float NDF = DistributionGGX(normal, halfway, roughness);
        const float G = GeometrySmith(normal, viewDirection, lightDirection, roughness);
        const vec3 numerator = NDF * G * F;
        const float denominator = 4.f * max(dot(normal, viewDirection), .0f) * max(dot(normal, lightDirection), .0f) + 0.0001;
        const vec3 specular = numerator / denominator;

        const vec3 kS = F;
        const vec3 kD = (vec3(1.f) - kS) * (1.f - metalness);

        const uint matrixIndex = min(lights[lightIndex].matrixIndex, DIRECTIONAL_LIGHT_COUNT - 1);
        vec4 lightSpacePosition = matrices[matrixIndex] * vec4(worldPosition, 1.f);
        lightSpacePosition /= lightSpacePosition.w;
        const vec3 shadowMapUV = vec3(lightSpacePosition.xy * .5f + .5f, lightSpacePosition.z);
        const float shadow = isDirectional * texture(sampler2DShadow(directionalShadowMaps[matrixIndex], shadowSampler), shadowMapUV) + isPoint * 1.f;

        float NdotL = max(dot(normal, lightDirection), .0f);
        Lo += shadow * (kD * albedoColour.rgb / PI + specular) * irradiance * NdotL;
    }

    vec3 ambient = vec3(.03f) * albedoColour.rgb;
    vec3 colour = ambient + Lo;
    colour = colour / (colour + vec3(1.0));
    colour = pow(colour, vec3(1.0/2.2));

    outColour = vec4(colour, 1.f);
}