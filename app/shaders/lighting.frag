#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions: require

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColour;

layout (set = 0, binding = 0) uniform sampler samp;
layout (set = 0, binding = 1) uniform texture2D textures[];
layout (set = 0, binding = 1) uniform textureCube cubemaps[];
layout (set = 2, binding = 0) uniform texture2D albedo;
layout (set = 2, binding = 1) uniform texture2D material;
layout (set = 2, binding = 2) uniform texture2D depthBuffer;
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
    uint shadowMapIndex;
    uint matrixIndex;
};

layout (constant_id = 0) const uint LIGHT_COUNT = 1u;
layout (constant_id = 1) const uint DIRECTIONAL_LIGHT_COUNT = 1u;
layout (constant_id = 2) const uint POINT_LIGHT_COUNT = 0u;
layout (constant_id = 3) const bool ENABLE_DIRECTIONAL_LIGHT = true;
layout (constant_id = 4) const bool ENABLE_POINT_LIGHT = true;
layout (constant_id = 5) const float SHADOW_FAR_PLANE = 100.f;
layout (std430, set = 1, binding = 1) readonly buffer LightsSSBO
{
    Light lights[LIGHT_COUNT];
};
layout (std430, set = 1, binding = 2) readonly buffer LightMatricesSSBO
{
    mat4 matrices[DIRECTIONAL_LIGHT_COUNT];
};
layout (set = 1, binding = 3) uniform sampler shadowSampler;

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

vec3 CalculateLight(vec3 viewDirection, vec3 lightDirection, vec3 normal, vec3 lightColour, vec3 albedoColour, float roughness, float metalness, float illuminance)
{
    const vec3 F0 = mix(vec3(.04f), albedoColour.rgb, metalness);
    const vec3 halfway = normalize(viewDirection + lightDirection);

    const float NdotL = max(dot(normal, lightDirection), .0f);
    const vec3 radiance = lightColour.rgb * illuminance * NdotL;
    const vec3 F = FresnelSchlick(max(dot(halfway, viewDirection), .0f), F0);
    const float NDF = DistributionGGX(normal, halfway, roughness);
    const float G = GeometrySmith(normal, viewDirection, lightDirection, roughness);
    const vec3 numerator = NDF * G * F;
    const float denominator = 4.f * max(dot(normal, viewDirection), .0f) * max(dot(normal, lightDirection), .0f) + 0.0001;
    const vec3 specular = numerator / denominator;

    const vec3 kS = F;
    const vec3 kD = (vec3(1.f) - kS) * (1.f - metalness);
    return (kD * albedoColour.rgb / PI + specular) * radiance;
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
    if (ENABLE_DIRECTIONAL_LIGHT)
    for (uint lightIndex = 0u; lightIndex < DIRECTIONAL_LIGHT_COUNT; ++lightIndex)
    {
        const vec3 lightDirection = lights[lightIndex].position.xyz;

        const float illuminance = lights[lightIndex].colour.a;

        const uint matrixIndex = min(lights[lightIndex].matrixIndex, DIRECTIONAL_LIGHT_COUNT - 1);
        vec4 lightSpacePosition = matrices[matrixIndex] * vec4(worldPosition, 1.f);
        lightSpacePosition /= lightSpacePosition.w;
        const vec3 shadowMapUV = vec3(lightSpacePosition.xy * .5f + .5f, lightSpacePosition.z);
        const float shadow = texture(sampler2DShadow(textures[lights[lightIndex].shadowMapIndex], shadowSampler), shadowMapUV);

        Lo += shadow * CalculateLight(viewDirection, lightDirection, normal, lights[lightIndex].colour.rgb, albedoColour.rgb, roughness, metalness, illuminance);
    }

    if (ENABLE_POINT_LIGHT)
    for (uint lightIndex = LIGHT_COUNT - POINT_LIGHT_COUNT; lightIndex < LIGHT_COUNT; ++lightIndex)
    {
        const vec3 lightDirection = normalize((lights[lightIndex].position.xyz - worldPosition));

        const float lumen = lights[lightIndex].colour.a;
        const float luminousIntensity = lumen / (4.f * PI);
        const float distance = length(lights[lightIndex].position.xyz - worldPosition);
        const float attenuation = 1.f / max(distance * distance, 0.0001f);

        const float illuminance = luminousIntensity * attenuation;

        vec3 fragToLight = -lights[lightIndex].position.xyz + worldPosition;
        float currentDepth = length(fragToLight) / SHADOW_FAR_PLANE;
        float shadow = texture(samplerCubeShadow(cubemaps[lights[lightIndex].shadowMapIndex], shadowSampler), vec4(fragToLight, currentDepth)).r;

        Lo += shadow * CalculateLight(viewDirection, lightDirection, normal, lights[lightIndex].colour.rgb, albedoColour.rgb, roughness, metalness, illuminance);
    }

    vec3 ambient = vec3(.03f) * albedoColour.rgb;
    vec3 colour = ambient + Lo;

    outColour = vec4(colour, 1.f);
}