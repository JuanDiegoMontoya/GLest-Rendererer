#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 1) uniform sampler2D gDepth;
layout (location = 2) uniform sampler2D shadowDepth;
layout (location = 3) uniform mat4 u_invViewProj;
layout (location = 4) uniform mat4 u_lightMatrix;
layout (location = 5) uniform sampler2D u_blueNoise;
layout (location = 6) uniform ivec2 u_screenSize;

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

const int INV_FREQ = 1;
const int NUM_STEPS = 50;
const float intensity = .02 * INV_FREQ * INV_FREQ;
const float distToFull = 20.0;

void main()
{
  if (mod(ivec2(gl_FragCoord.xy), ivec2(INV_FREQ)) != ivec2(0))
    discard;
  const vec3 rayEnd = WorldPosFromDepth(texture(gDepth, vTexCoord).r, u_screenSize, u_invViewProj);
  const vec3 rayStart = WorldPosFromDepth(0, u_screenSize, u_invViewProj);
  const vec3 rayDir = normalize(rayEnd - rayStart);
  const float rayStep = distance(rayEnd, rayStart) / NUM_STEPS;
  vec3 rayPos = rayStart + rayStep * texelFetch(u_blueNoise, ivec2(mod(ivec2(gl_FragCoord.xy) / INV_FREQ, textureSize(u_blueNoise, 0))), 0).x / 1.0;
  
  float accum = 0.0;
  for (int i = 0; i < NUM_STEPS; i++)
  {
    vec4 lightSpacePos = u_lightMatrix * vec4(rayPos, 1.0);
    accum += 1.0 - Shadow(lightSpacePos);
    rayPos += rayDir * rayStep;
  }

  fragColor = vec4(vec3((distance(rayEnd, rayStart) / distToFull) * intensity * (accum / NUM_STEPS)), 1.0);
}