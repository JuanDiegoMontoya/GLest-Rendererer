#version 460 core

layout (location = 0) out vec2 moments;

void main()
{
  moments.x = gl_FragCoord.z;
  moments.y = gl_FragCoord.z * gl_FragCoord.z;
}