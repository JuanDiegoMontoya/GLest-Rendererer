#version 460 core

layout (location = 0) in vec3 aPos;

//layout (location = 0) uniform mat4 u_model;
layout (location = 1) uniform mat4 u_viewProj;

layout (location = 0) out vec3 vLightPos;
layout (location = 1) out flat int vInstanceID;

struct PointLight
{
  vec4 diffuse;
  vec4 specular;

  vec4 position;
  float linear;
  float quadratic;
  float radiusSquared;
  float _padding;
};

layout (std430, binding = 0) readonly buffer lit
{
  PointLight lights[];
};

void main()
{
  vInstanceID = gl_InstanceID;
  vLightPos = aPos * sqrt(lights[vInstanceID].radiusSquared) + lights[vInstanceID].position.xyz;
  vec4 clip = u_viewProj * vec4(vLightPos, 1.0);
  //vTexCoord = (clip.xy + 1.0) / 2.0;
  gl_Position = clip;
}