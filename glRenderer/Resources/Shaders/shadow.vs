#version 460 core

layout (location = 0) in vec3 aPos;

layout (location = 0) uniform mat4 u_modelLight;

void main()
{
  gl_Position = u_modelLight * vec4(aPos, 1.0);
}