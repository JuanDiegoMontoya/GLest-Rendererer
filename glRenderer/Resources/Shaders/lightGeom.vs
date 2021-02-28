#version 460 core

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

layout (location = 0) in vec3 aPos;

layout (location = 1) uniform mat4 u_viewProj;

layout (location = 0) out flat PointLight vLight;

void main()
{
  vLight = lights[gl_InstanceID];
  vec3 vPos = aPos * sqrt(vLight.radiusSquared) + vLight.position.xyz;
  gl_Position = u_viewProj * vec4(vPos, 1.0);
}