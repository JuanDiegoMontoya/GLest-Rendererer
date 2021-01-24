#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) uniform mat4 u_model;
layout (location = 1) uniform mat4 u_viewProj;
layout (location = 2) uniform mat3 u_normalMatrix;

layout (location = 0) out vec3 vNormal;
layout (location = 1) out vec3 vPos;
layout (location = 2) out vec2 vTexCoord;

void main()
{
  vTexCoord = aTexCoord;
  vPos = vec3(u_model * vec4(aPos, 1.0));
  //vNormal = transpose(inverse(mat3(u_model))) * aNormal;
  vNormal = u_normalMatrix * aNormal;

  gl_Position = u_viewProj * vec4(vPos, 1.0);
}