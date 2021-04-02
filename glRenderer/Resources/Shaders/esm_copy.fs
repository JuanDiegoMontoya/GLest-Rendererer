#version 460 core

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D u_tex;
layout (location = 1) uniform float u_C;

layout (location = 0) out float expDepth;

void main()
{
  float depth = texture(u_tex, vTexCoord).r;
  expDepth = exp(u_C * depth);
}