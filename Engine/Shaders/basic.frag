#version 460 core
out vec4 FragColor;

in vec2 vTexCoord;
in vec3 vNormal;
in vec3 vFragPos;

uniform sampler2D uTexture;
uniform sampler2D uShadowMap;
uniform vec4 vertexColor;
uniform vec4 ambientLightColor;

uniform vec3 uLightDir;      // normalized, pointing FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uViewPos;       // camera world position, for specular

in vec4 vFragPosLightSpace;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec3 norm = normalize(vNormal);

    float diff = max(dot(norm, uLightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    float shadow = ShadowCalculation(vFragPosLightSpace, norm, uLightDir);
    vec3 lighting = ambientLightColor.rgb + (1.0 - shadow) * diffuse;

    vec4 texColor = texture(uTexture, vTexCoord) * vertexColor;
    FragColor = vec4(texColor.rgb * lighting, texColor.a);
}