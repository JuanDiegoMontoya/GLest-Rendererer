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

// render textures
layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec2 gNormal;
layout (location = 2) out float gShininess;

layout (location = 0) in vec3 vPos;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vTexCoord;
layout (location = 3) in mat3 vTBN;

layout (location = 3) uniform Material u_object;

void main()
{
  if (u_object.hasAlpha && texture(u_object.alpha, vTexCoord).r == 0.0)
  {
    discard;
  }
  vec3 normal = vTBN[2];
  if (u_object.hasNormal)
  {
    //if (normal == vec3(0.0)) // disables normal mapping
    {
      vec3 t = vTBN[0];
      vec3 b = vTBN[1];
      vec3 n = vTBN[2];
      t = t - n * dot(t, n); // orthonormalization ot the tangent vectors
      b = b - n * dot(b, n); // orthonormalization of the binormal vectors to the normal vector 
      b = b - t * dot(b, t); // orthonormalization of the binormal vectors to the tangent vector
      mat3 TBN = mat3(normalize(t), normalize(b), n);
      normal = texture(u_object.normal, vTexCoord).rgb * 2.0 - 1.0;
      normal = normalize(TBN * normal);
    }
  }
  gNormal = float32x3_to_oct(normalize(normal));
  //gNormal.xyz = normalize(normal);
#if VISUALIZE_MAPS
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