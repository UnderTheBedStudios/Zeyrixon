#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uLightSpaceMatrix;

out vec2 vTexCoord;
out vec3 vNormal;
out vec3 vFragPos;
out vec4 vFragPosLightSpace;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;

    vFragPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vTexCoord = aTexCoord;
    vFragPosLightSpace = uLightSpaceMatrix * worldPos;
}