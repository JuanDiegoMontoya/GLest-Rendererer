#version 460 core

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D u_tex;

layout (location = 0) out vec4 outMoments;

void main()
{
  float depth = texture(u_tex, vTexCoord).r;
  outMoments.xyzw = vec4(depth, depth * depth, depth * depth * depth, depth * depth * depth * depth);
}