#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 1, binding = 1) uniform sampler2D gDepth;
layout (location = 2, binding = 2) uniform sampler2D shadowDepth;
layout (location = 3, binding = 3) uniform sampler2D u_blueNoise;
layout (location = 4) uniform mat4 u_invViewProj;
layout (location = 5) uniform mat4 u_lightMatrix;
layout (location = 6) uniform ivec2 u_screenSize;
layout (location = 8) uniform int NUM_STEPS = 32;
layout (location = 9) uniform float intensity = .025;
layout (location = 10) uniform float noiseOffset = 1.0;
layout (location = 11) uniform float u_beerPower = 1.0;
layout (location = 12) uniform float u_powderPower = 1.0;
layout (location = 13) uniform float u_distanceScale = 1.0;
layout (location = 14) uniform float u_heightOffset = 0.0;
layout (location = 15) uniform float u_hfIntensity = 1.0;

layout (location = 0) out float fragColor;

float Shadow(vec4 lightSpacePos)
{
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  if (projCoords.z > 1.0) return 1.0;
  projCoords = projCoords * 0.5 + 0.5;
  if (any(lessThan(projCoords.xy, vec2(0))) || any(greaterThan(projCoords.xy, vec2(1)))) return 1.0;
  float closestDepth = texture(shadowDepth, projCoords.xy).r;

  float currentDepth = projCoords.z;
  float shadow = currentDepth > closestDepth ? 1.0 : 0.0;

  return shadow;
}

void main()
{
  float depth = texture(gDepth, vTexCoord).r;
  const vec3 rayEnd = WorldPosFromDepth(depth, u_screenSize, u_invViewProj);
  const vec3 rayStart = WorldPosFromDepth(0, u_screenSize, u_invViewProj);
  const vec3 rayDir = normalize(rayEnd - rayStart);
  const float totalDistance = distance(rayStart, rayEnd);
  const float rayStep = totalDistance / NUM_STEPS;
  const vec2 noiseUV = gl_FragCoord.xy / textureSize(u_blueNoise, 0);
  vec3 rayPos = rayStart + rayDir * rayStep * texture(u_blueNoise, noiseUV).x * noiseOffset;
  
  float accum = 0.0;
  for (int i = 0; i < NUM_STEPS; i++)
  {
    vec4 lightSpacePos = u_lightMatrix * vec4(rayPos, 1.0);
    accum += 1.0 - Shadow(lightSpacePos);
    rayPos += rayDir * rayStep;
  }

  const float d = accum * rayStep * intensity;
  const float powder = 1.0 - exp(-d * 2.0 * u_powderPower);
  const float beer = exp(-d * u_beerPower);

  fragColor = (1.0 - beer) * powder;

  // the fog function is: f(x, y, z) = 1/(y)
  // TODO: add clamp to fix edge cases
  // TODO: handle rayEnd at far plane
  vec3 rayStartC = rayStart;
  vec3 rayEndC = rayEnd;
  rayEndC += rayDir * u_distanceScale;
  rayStartC.y = clamp(rayStartC.y + u_heightOffset, .001, 10000.0);
  rayEndC.y = clamp(rayEndC.y + u_heightOffset, .001, 10000.0);
  float mag = length(rayEndC - rayStartC);
  float scatter = mag * (1.0 / (rayStartC.y - rayEndC.y)) * (log(abs(rayStartC.y)) - log(abs(rayEndC.y)));
  fragColor += scatter * u_hfIntensity;
}