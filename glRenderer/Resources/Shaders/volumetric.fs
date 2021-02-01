#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D u_hdrBuffer;
layout (location = 1) uniform sampler2D gDepth;
layout (location = 2) uniform sampler2D shadowDepth;
layout (location = 3) uniform mat4 u_invViewProj;
layout (location = 4) uniform mat4 u_lightMatrix;

layout (location = 0) out vec4 fragColor;

float Shadow(vec4 lightSpacePos)
{
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  if(projCoords.z > 1.0) return 0.0;
  projCoords = projCoords * 0.5 + 0.5;
  float closestDepth = texture(shadowDepth, projCoords.xy).r;

  float currentDepth = projCoords.z;
  float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

  return shadow;
}

const int NUM_STEPS = 100;
const float intensity = .02;
const float distToFull = 20.0;

#if 1
void main()
{
  const vec3 rayEnd = WorldPosFromDepth(texture(gDepth, vTexCoord).r, textureSize(gDepth, 0), u_invViewProj);
  const vec3 rayStart = WorldPosFromDepth(0, textureSize(gDepth, 0), u_invViewProj);
  const vec3 rayDir = normalize(rayEnd - rayStart);
  const float rayStep = length(rayEnd - rayStart) / NUM_STEPS;
  vec3 rayPos = rayStart;

  float accum = 0.0;
  for (int i = 0; i < NUM_STEPS; i++)
  {
    vec4 lightSpacePos = u_lightMatrix * vec4(rayPos, 1.0);
    accum += 1.0 - Shadow(lightSpacePos);
    rayPos += rayDir * rayStep;
  }

  fragColor = vec4((length(rayEnd - rayStart) / distToFull) * intensity * (accum / NUM_STEPS) + texture(u_hdrBuffer, vTexCoord).rgb, 1.0);
}
#else
void main()
{
  float rayEnd = texture(gDepth, vTexCoord).r;
  float rayPos = 0.0;

}
#endif