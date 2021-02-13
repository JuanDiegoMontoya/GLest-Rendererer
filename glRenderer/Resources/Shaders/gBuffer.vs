#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

layout (location = 0) uniform mat4 u_model;
layout (location = 1) uniform mat4 u_viewProj;
layout (location = 2) uniform mat3 u_normalMatrix;

layout (location = 0) out vec3 vPos;
layout (location = 1) out vec3 vNormal;
layout (location = 2) out vec2 vTexCoord;
layout (location = 3) out mat3 TBN;

void main()
{
  vTexCoord = aTexCoord;
  vPos = vec3(u_model * vec4(aPos, 1.0));
  vNormal = u_normalMatrix * aNormal;
  vec3 T = normalize(vec3(u_model * vec4(aTangent, 0.0)));
  vec3 N = normalize(vec3(u_model * vec4(aNormal, 0.0)));
  T = normalize(T - dot(T, N) * N);
  vec3 B = cross(N, T);
  //vec3 B = normalize(vec3(u_model * vec4(aBitangent, 0.0)));
  TBN = mat3(T, B, N);
  gl_Position = u_viewProj * vec4(vPos, 1.0);
}