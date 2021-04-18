#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;

layout (location = 0) uniform mat4 u_viewProj;

struct ObjectUniforms
{
  mat4 modelMatrix;
  uint materialIndex;
};

layout (binding = 0, std430) readonly buffer uniforms
{
  ObjectUniforms objects[];
};

layout (location = 0) out VS_OUT
{
  vec3 vNormal;
  vec2 vTexCoord;
  flat uint vMaterialIndex;
};

void main()
{
  ObjectUniforms obj = objects[gl_DrawID];
  vMaterialIndex = obj.materialIndex;
  vTexCoord = aTexCoord;
  vec3 wPos = vec3(obj.modelMatrix * vec4(aPos, 1.0));
  vNormal = vec3(obj.modelMatrix * vec4(aNormal, 0.0));
  //vNormal = mat3(obj.normalMatrix) * aNormal;
  gl_Position = u_viewProj * vec4(wPos, 1.0);
}