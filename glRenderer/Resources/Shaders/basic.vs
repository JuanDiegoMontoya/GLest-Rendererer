#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) uniform mat4 u_ViewProj;
layout (location = 1) uniform mat4 u_Model;
layout (location = 2) uniform mat4 u_NormalMatrix;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec2 vTexCoord;
layout (location = 2) out vec3 vNormal;

void main()
{
  vTexCoord = aTexCoord;

  vPos = vec3(u_Model * vec4(aPos, 1.0f));
  vNormal = aNormal;

  gl_Position = u_ViewProj * vec4(vPos, 1.0);
}