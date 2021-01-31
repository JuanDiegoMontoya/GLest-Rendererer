#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D u_hdrBuffer;
layout (location = 1) uniform sampler2D gDepth;
layout (location = 2) uniform sampler2D shadowDepth;
layout (location = 3) uniform mat4 u_invView;
layout (location = 4) uniform mat4 u_invProj;
layout (location = 5) uniform mat4 u_lightMatrix;

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

void main()
{
  const vec3 rayEnd = WorldPosFromDepth(texture(gDepth, vTexCoord).r, textureSize(gDepth, 0), u_invProj, u_invView);
  vec3 rayPos = WorldPosFromDepth(0, textureSize(gDepth, 0), u_invProj, u_invView);
  const vec3 rayDir = normalize(rayEnd - rayPos);
  const int NUM_STEPS = 50;
  const float rayStep = length(rayEnd - rayPos) / NUM_STEPS;
  const float intensity = .02;

  float accum = 0.0;
  for (int i = 0; i < NUM_STEPS; i++)
  {
    vec4 lightSpacePos = u_lightMatrix * vec4(rayPos, 1.0);
    accum += 1.0 - Shadow(lightSpacePos);
    rayPos += rayDir * rayStep;
  }

  fragColor = vec4(vec3(intensity*accum/NUM_STEPS) + texture(u_hdrBuffer, vTexCoord).rgb, 1.0);
}