#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D gColor;
layout (location = 1) uniform float c_phi;
layout (location = 2) uniform float stepwidth;
layout (location = 3) uniform ivec2 u_resolution;
layout (location = 4) uniform float kernel[25];
layout (location = 29) uniform vec2 offsets[25];

layout (location = 0) out vec4 fragColor;

void main()
{
  vec4 sum = vec4(0.0);
  vec2 step = 1.0 / u_resolution;
  vec4 cval = texture(gColor, vTexCoord);

  float cum_w = 0.0;
  for (int i = 0; i < 25; i++)
  {
    vec2 uv = vTexCoord + offsets[i] * step * stepwidth;

    vec4 ctmp = texture(gColor, uv);
    vec4 t = cval - ctmp;
    float dist2 = dot(t, t);
    float c_w = min(exp(-(dist2) / c_phi), 1.0);

    float weight = c_w;
    sum += ctmp * weight * kernel[i];
    cum_w += weight * kernel[i];
  }

  fragColor = sum / cum_w;
}