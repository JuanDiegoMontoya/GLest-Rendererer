#version 460 core
#include "common.h"

#define VISUALIZE_MAPS 0

struct Material
{
  sampler2D diffuse;
  sampler2D alpha;
  sampler2D specular;
  sampler2D normal;
  bool hasSpecular;
  bool hasAlpha;
  bool hasNormal;
  float shininess;
};

// texture buffers
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gAlbedoSpec;
layout (location = 2) out vec2 gNormal;
layout (location = 3) out float gShininess;

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;
layout (location = 3) in mat3 TBN;

layout (location = 3) uniform Material u_object;

void main()
{
  if (u_object.hasAlpha && texture(u_object.alpha, vTexCoord).r == 0.0)
  {
    discard;
  }
  vec3 normal = vNormal;
  if (u_object.hasNormal)
  {
    normal = texture(u_object.normal, vTexCoord).rgb * 2.0 - 1.0;
    normal = TBN * normal;
  }
  gPosition = vec4(vPos, 1.0);
  gNormal = float32x3_to_oct(normalize(normal));
  //gNormal.xyz = normalize(normal);
#if VISUALIZE_MAPS
  gPosition = vec4(normalize(vPos) * .5 + .5, 1.0);
  //gNormal = float32x3_to_oct(normalize(normal) * .5 + .5);
  gNormal.xyz = (normalize(normal) * .5 + .5);
#endif
  gAlbedoSpec.rgb = texture(u_object.diffuse, vTexCoord).rgb;
  gAlbedoSpec.a = 0.0;
  if (u_object.hasSpecular)
  {
    gAlbedoSpec.a = texture(u_object.specular, vTexCoord).r;
  }
  gShininess.r = u_object.shininess;
}