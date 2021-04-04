#version 460
#include "common.h"
#define M_PI 3.1415926536
#define M_TAU (2.0 * M_PI)

layout (location = 0) uniform mat4 u_invViewProj;
layout (location = 1) uniform ivec2 u_screenSize;
layout (location = 2) uniform vec3 u_camPos;
layout (location = 3, binding = 0) uniform sampler2D u_hdriTexture;

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) out vec4 fragColor;

vec2 NormToEquirectangularUV(vec3 normal)
{
  // float phi = (-0.6981 * -normal.y * -normal.y - 0.8726) * -normal.y + 1.5707;
  // float theta = normal.xz * (-0.1784 * normal.xz - 0.0663 * normal.xz * normal.xz + 1.0301);
  // vec2 uv = vec2(theta * 0.1591, phi * 0.3183);
  float phi = acos(-normal.y);
  float theta = atan(normal.z, normal.x) + M_PI; // or normal.xz?
  vec2 uv = vec2(theta / M_TAU, phi / M_PI);
  return uv;
}

void main()
{
  vec3 dir = normalize(WorldPosFromDepth(1.0, u_screenSize, u_invViewProj) - u_camPos);
  vec2 uv = NormToEquirectangularUV(dir);
  fragColor = vec4(texture(u_hdriTexture, uv).rgb, 1.0);
}