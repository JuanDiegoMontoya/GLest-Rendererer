#version 460 core
#extension GL_ARB_bindless_texture : enable
#include "common.h"

struct Material
{
  uvec2 albedoHandle;
  uvec2 roughnessHandle;
  uvec2 metalnessHandle;
  uvec2 normalHandle;
  uvec2 ambientOcclusionHandle;
};

layout (location = 3) uniform bool u_materialOverride;
layout (location = 4) uniform vec3 u_albedoOverride;
layout (location = 5) uniform float u_roughnessOverride;
layout (location = 6) uniform float u_metalnessOverride;
layout (location = 7) uniform bool u_AOoverride;
layout (location = 8) uniform float u_ambientOcclusionOverride;

layout (binding = 1, std430) readonly buffer Materials
{
  Material materials[];
};

layout (location = 0) in VS_OUT
{
  vec3 vNormal;
  vec2 vTexCoord;
  flat uint vMaterialIndex;
  //mat3 TBN;
};

layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec2 gNormal;
layout (location = 2) out vec4 gRMA;

void main()
{
  Material material = materials[vMaterialIndex];
  vec3 normal = normalize(vNormal);
  const bool hasAlbedo = (material.albedoHandle.x != 0 || material.albedoHandle.y != 0);
  const bool hasRoughness = (material.roughnessHandle.x != 0 || material.roughnessHandle.y != 0);
  const bool hasMetalness = (material.metalnessHandle.x != 0 || material.metalnessHandle.y != 0);
  const bool hasNormal = (material.normalHandle.x != 0 || material.normalHandle.y != 0);
  const bool hasAmbientOcclusion = (material.ambientOcclusionHandle.x != 0 || material.ambientOcclusionHandle.y != 0);
  gNormal = float32x3_to_oct(normalize(normal));
  vec4 color = vec4(0.1, 0.1, 0.1, 1);
  if (hasAlbedo)
  {
    color = texture(sampler2D(material.albedoHandle), vTexCoord).rgba;
  }
  if (color.a <= .01)
  {
    discard;
  }
  gAlbedo.rgb = color.rgb;
  gAlbedo.a = 1.0; // unused
  gRMA.rgba = vec4(1.0, 0.0, 1.0, 1.0); // sane defaults, gRMA.a is unused
  if (!u_materialOverride)
  {
    if (hasRoughness)
    {
      gRMA[0] = texture(sampler2D(material.roughnessHandle), vTexCoord).r;
    }
    if (hasMetalness)
    {
      gRMA[1] = texture(sampler2D(material.metalnessHandle), vTexCoord).r;
    }
    if (hasAmbientOcclusion)
    {
      gRMA[2] = texture(sampler2D(material.ambientOcclusionHandle), vTexCoord).r;
    }
  }
  else
  {
    gAlbedo.rgb = u_albedoOverride;
    gRMA[0] = u_roughnessOverride;
    gRMA[1] = u_metalnessOverride;
    gRMA[2] = u_ambientOcclusionOverride;
  }
  if (u_AOoverride)
  {
    gRMA[2] = u_ambientOcclusionOverride;
  }
}