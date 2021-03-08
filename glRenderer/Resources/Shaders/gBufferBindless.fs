#version 460 core
#extension GL_ARB_bindless_texture : enable
#include "common.h"

struct Material
{
  uvec2 diffuseHandle;
  uvec2 specularHandle;
  uvec2 normalHandle;
  float shininess;
  float _pad00;
};

layout (binding = 1, std430) readonly buffer Materials
{
  Material materials[];
};

layout (location = 0) in VS_OUT
{
  vec3 vPos;
  vec3 vNormal;
  vec2 vTexCoord;
  flat uint vMaterialIndex;
  //mat3 TBN;
};

layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec2 gNormal;
layout (location = 2) out float gShininess;

void main()
{
  Material material = materials[vMaterialIndex];
  vec3 normal = normalize(vNormal);
  const bool hasDiffuse = (material.diffuseHandle.x != 0 || material.diffuseHandle.y != 0);
  const bool hasSpecular = (material.specularHandle.x != 0 || material.specularHandle.y != 0);
  const bool hasNormal = (material.normalHandle.x != 0 || material.normalHandle.y != 0);
  gNormal = float32x3_to_oct(normalize(normal));
  vec4 color = vec4(0, 0, 0, 1);
  if (hasDiffuse)
  {
    color = texture(sampler2D(material.diffuseHandle), vTexCoord).rgba;
  }
  if (color.a <= .01)
  {
    discard;
  }
  gAlbedoSpec.rgb = color.rgb;
  gAlbedoSpec.a = 0.0;
  if (hasSpecular)
  {
    gAlbedoSpec.a = texture(sampler2D(material.specularHandle), vTexCoord).r;
  }
  gShininess.r = material.shininess;
}