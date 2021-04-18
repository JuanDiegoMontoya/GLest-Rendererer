#version 460 core
#include "common.h"
#define M_PI 3.14159265359

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D gDepth;
layout (location = 1, binding = 1) uniform sampler2D gNormal;
layout (location = 2) uniform mat4 u_invViewProj;
layout (location = 3) uniform mat4 u_view;
layout (location = 4) uniform uint u_numSamples;
layout (location = 5) uniform float u_delta;
layout (location = 6) uniform float u_R; // range of influence
layout (location = 8) uniform float u_s; // constant coefficient
layout (location = 9) uniform float u_k; // constant power

layout (location = 0) out float ambientOcclusion;

float hstep(float x)
{
  return x > 0 ? 1 : 0;
}

void main()
{
  float ndcDepth = texture(gDepth, vTexCoord).x;
  if (ndcDepth == 1.0)
  {
    discard;
  }

  vec3 P = WorldPosFromDepthUV(ndcDepth, vTexCoord, u_invViewProj);
  vec3 N = oct_to_float32x3(texture(gNormal, vTexCoord).xy);
  float d = -(u_view * vec4(P, 1.0)).z; // camera-space depth
  float c = 0.1 * u_R;
  
  ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  float phi = 30.0 * float(fragCoord.x ^ fragCoord.y) + 10.0 * float(fragCoord.x * fragCoord.y);

  float sum = 0.0;
  for (uint i = 0; i < u_numSamples; i++)
  {
    // generate sample
    float alpha = (float(i) + 0.5) / float(u_numSamples);
    float h = (alpha * u_R) / d;
    float theta = 2.0 * M_PI * alpha * (7.0 * float(u_numSamples)) / 9.0 + phi;
    vec2 uv = fract(vTexCoord + h * vec2(cos(theta), sin(theta)));

    vec3 Pi = WorldPosFromDepthUV(texture(gDepth, uv).x, uv, u_invViewProj); // sample world position
    float di = -(u_view * vec4(Pi, 1.0)).z; // world/view-space sample depth
    vec3 omega_i = Pi - P; // based on Pi

    float numerator = max(0.0, dot(N, omega_i) - u_delta * di) * hstep(u_R - length(omega_i));
    float denom = max(c * c, dot(omega_i, omega_i));

    sum += numerator / denom;
  }
  sum *= (2.0 * M_PI * c) / float(u_numSamples);

  ambientOcclusion = max(0.0, pow(1.0 - u_s * sum, u_k));
}