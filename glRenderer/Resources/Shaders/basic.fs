#version 460 core

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec2 vTexCoord;
layout (location = 2) in vec3 vNormal;

layout (location = 0) out vec4 fragColor;

void main()
{
  fragColor = vec4(vNormal * .5 + .5, 1.0);
}