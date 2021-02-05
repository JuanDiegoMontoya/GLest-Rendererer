#version 460 core

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D u_tex;

layout (location = 0) out vec4 fragColor;

void main()
{
  fragColor = texture(u_tex, vTexCoord);
}