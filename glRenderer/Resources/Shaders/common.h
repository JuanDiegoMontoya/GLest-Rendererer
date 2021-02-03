#ifndef COMMON_H
#define COMMON_H

// Returns ±1
vec2 signNotZero(vec2 v)
{
  return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

// Assume normalized input. Output is on [-1, 1] for each component.
vec2 float32x3_to_oct(in vec3 v)
{
  // Project the sphere onto the octahedron, and then onto the xy plane
  vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
  // Reflect the folds of the lower hemisphere over the diagonals
  return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * signNotZero(p)) : p;
}

vec3 oct_to_float32x3(vec2 e)
{
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * signNotZero(v.xy);
  return normalize(v);
}

vec3 WorldPosFromDepth(float depth, vec2 screenSize, mat4 invViewProj)
{
  float z = depth * 2.0 - 1.0; // [0, 1] -> [-1, 1]
  vec2 normalized = gl_FragCoord.xy / screenSize; // [0.5, u_viewPortSize] -> [0, 1]
  vec4 clipSpacePosition = vec4(normalized * 2.0 - 1.0, z, 1.0); // [0, 1] -> [-1, 1]

  // undo view + projection
  vec4 worldSpacePosition = invViewProj * clipSpacePosition;
  worldSpacePosition /= worldSpacePosition.w;

  return worldSpacePosition.xyz;
}

#endif