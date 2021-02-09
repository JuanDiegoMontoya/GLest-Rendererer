#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D gColor;
layout (location = 1, binding = 1) uniform sampler2D gDepth;
layout (location = 2, binding = 2) uniform sampler2D gShininess;
layout (location = 3, binding = 3) uniform sampler2D gNormal;
layout (location = 4) uniform mat4 u_proj;
layout (location = 5) uniform mat4 u_view;
layout (location = 6) uniform mat4 u_invViewProj;
layout (location = 7) uniform float rayStep;
layout (location = 8) uniform float minRayStep;
layout (location = 9) uniform float thickness;
layout (location = 10) uniform float searchDist;
layout (location = 11) uniform int maxSteps;
layout (location = 12) uniform int binarySearchSteps;
layout (location = 13, binding = 4) uniform sampler2D u_blueNoise;
layout (location = 14) uniform ivec2 u_viewportSize;

layout (location = 0) out vec4 fragColor;


vec3 BinarySearch(vec3 dir, inout vec3 hit, out float dDepth)
{
  float viewDepth;
  for (int i = 0; i < binarySearchSteps; i++)
  {
    vec4 projected = u_proj * vec4(hit, 1.0);
    projected /= projected.w;
    projected = projected * 0.5 + 0.5;
    vec3 worldPosition = WorldPosFromDepth(texture(gDepth, projected.xy).x, u_viewportSize, u_invViewProj);
    viewDepth = (u_view * vec4(worldPosition, 1.0)).z;

    dDepth = hit.z - viewDepth;
    if (dDepth > 0.0) hit += dir;
    dir *= 0.5;
    hit -= dir;
  }

  vec4 projected = u_proj * vec4(hit, 1.0);
  projected /= projected.w;
  projected = projected * 0.5 + 0.5;
  return vec3(projected.xy, viewDepth);
}

vec4 Raycast(vec3 dir, inout vec3 hit, out float dDepth)
{
  dir *= rayStep;
  
  hit += dir * texelFetch(u_blueNoise, ivec2(mod(ivec2(gl_FragCoord.xy), textureSize(u_blueNoise, 0))), 0).x;

  for (int i = 0; i < maxSteps; i++)
  {
    hit += dir;

    vec4 projected = u_proj * vec4(hit, 1.0);
    projected.xy /= projected.w;
    projected.xy = projected.xy * 0.5 + 0.5; // [-1, 1] -> [0, 1]
    vec3 worldPosition = WorldPosFromDepth(texture(gDepth, projected.xy).x, u_viewportSize, u_invViewProj);
    float viewDepth = (u_view * vec4(worldPosition, 1.0)).z;

    dDepth = hit.z - viewDepth;
    if (dDepth < -length(dir))
    {
      return vec4(0.0);
    }
    if (dDepth < thickness)
    {
      return vec4(BinarySearch(dir, hit, dDepth), 1.0);
    }
  }

  return vec4(0.0);
}

void main()
{
  float shininess = texture(gShininess, vTexCoord).r;
  if (shininess <= 1.0)
  {
    discard;
  }

  vec3 worldPosition = WorldPosFromDepth(texture(gDepth, vTexCoord).x, u_viewportSize, u_invViewProj);
  vec3 worldNormal = oct_to_float32x3(texture(gNormal, vTexCoord).xy);

  vec3 viewPosition = (u_view * vec4(worldPosition, 1.0)).xyz;
  vec3 viewNormal = normalize((u_view * vec4(worldNormal, 0.0)).xyz);
  vec3 reflected = normalize(reflect(normalize(viewPosition), normalize(viewNormal)));

  vec3 hit = viewPosition;
  float dDepth = 0.0;

  vec4 coords = Raycast(reflected * max(minRayStep, -viewPosition.z), hit, dDepth);
  
  vec2 dCoords = abs(vec2(0.5) - coords.xy);
  float screenEdgeFactor = clamp(1.0 - (dCoords.x + dCoords.y), 0.0, 1.0);

  vec3 startColor = texture(gColor, vTexCoord).rgb;
  fragColor = vec4(texture(gColor, coords.xy).rgb, 1.0) * 
    screenEdgeFactor *
    clamp(-reflected.z, 0.0, 1.0) *
    clamp((searchDist - distance(viewPosition, hit)) * (1 / searchDist), 0.0, 1.0) * 
    coords.w;
  //fragColor = mix(fragColor, vec4(startColor, 1.0), .1);
  //fragColor = fragColor * .0001 + vec4(viewNormal, 1.0); // debug
}