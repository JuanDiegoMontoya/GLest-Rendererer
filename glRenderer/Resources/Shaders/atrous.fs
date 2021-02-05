#version 460 core
#include "common.h"

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D gColor;
layout (location = 1) uniform sampler2D gDepth;
layout (location = 2) uniform sampler2D gNormal;
layout (location = 3) uniform float c_phi;
layout (location = 4) uniform float n_phi;
layout (location = 5) uniform float p_phi;
layout (location = 6) uniform float stepwidth;
layout (location = 7) uniform mat4 u_invViewProj;
layout (location = 8) uniform ivec2 u_resolution;
layout (location = 9) uniform float kernel[25];
layout (location = 34) uniform vec2 offsets[25];

layout (location = 0) out vec4 fragColor;

void main()
{
  vec4 sum = vec4(0.0);
  vec2 step = 1.0 / u_resolution;
  vec4 cval = texture(gColor, vTexCoord);
  vec4 nval = vec4(oct_to_float32x3(texture(gNormal, vTexCoord).xy), 0.0);
  vec4 pval = vec4(WorldPosFromDepth(texture(gDepth, vTexCoord).r, textureSize(gDepth, 0), u_invViewProj), 1.0);

  float cum_w = 0.0;
  for (int i = 0; i < 25; i++)
  {
    vec2 uv = vTexCoord + offsets[i] * step * stepwidth;

    vec4 ctmp = texture(gColor, uv);
    vec4 t = cval - ctmp;
    float dist2 = dot(t, t);
    float c_w = min(exp(-(dist2) / c_phi), 1.0);

    vec4 ntmp = vec4(oct_to_float32x3(texture(gNormal, uv).xy), 0.0);
    t = nval - ntmp;
    dist2 = max(dot(t, t) / (stepwidth * stepwidth), 0.0);
    float n_w = min(exp(-(dist2) / n_phi), 1.0);
    
    vec4 ptmp = vec4(WorldPosFromDepth(texture(gDepth, uv).r, textureSize(gDepth, 0), u_invViewProj), 1.0);
    t = pval - ptmp;
    dist2 = dot(t, t);
    float p_w = min(exp(-(dist2) / p_phi), 1.0);

    float weight = c_w * n_w * p_w;
    sum += ctmp * weight * kernel[i];
    cum_w += weight * kernel[i];
  }

  fragColor = sum / cum_w;
}