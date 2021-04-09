#version 460
#include "common.h"
#include "pbr_common.h"


layout (location = 0) uniform mat4 u_invViewProj;
layout (location = 1) uniform ivec2 u_screenSize;
layout (location = 2) uniform vec3 u_camPos;
layout (location = 3, binding = 0) uniform sampler2D u_hdriTexture;

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) out vec4 fragColor;

void main()
{
  vec3 dir = normalize(WorldPosFromDepth(1.0, u_screenSize, u_invViewProj) - u_camPos);
  vec2 uv = NormToEquirectangularUV(dir);
  fragColor = vec4(textureLod(u_hdriTexture, uv, 0.0).rgb, 1.0);
}