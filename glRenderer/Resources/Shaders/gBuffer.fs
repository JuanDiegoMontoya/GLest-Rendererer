#version 460 core
#extension GL_ARB_bindless_texture : enable
#include "common.h"

#define VISUALIZE_MAPS 0

layout (location = 3, bindless_sampler) uniform sampler2D diffuse;
layout (location = 4, bindless_sampler) uniform sampler2D specular;
layout (location = 5, bindless_sampler) uniform sampler2D normal;
layout (location = 6) uniform bool hasSpecular;
layout (location = 7) uniform bool hasNormal;
layout (location = 8) uniform float shininess;

layout (location = 0) in VS_OUT
{
  vec3 vPos;
  vec3 vNormal;
  vec2 vTexCoord;
  //mat3 TBN;
};

layout (location = 0) out vec4 gAlbedoSpec;
layout (location = 1) out vec2 gNormal;
layout (location = 2) out float gShininess;

void main()
{
  //vec3 normal = vTBN[2];
  vec3 normal = normalize(vNormal);
  if (hasNormal)
  {
    //if (normal == vec3(0.0)) // disables normal mapping
    // {
    //   vec3 t = vTBN[0];
    //   vec3 b = vTBN[1];
    //   vec3 n = vTBN[2];
    //   t = t - n * dot(t, n); // orthonormalization ot the tangent vectors
    //   b = b - n * dot(b, n); // orthonormalization of the binormal vectors to the normal vector 
    //   b = b - t * dot(b, t); // orthonormalization of the binormal vectors to the tangent vector
    //   mat3 TBN = mat3(normalize(t), normalize(b), n);
    //   normal = texture(normal, vTexCoord).rgb * 2.0 - 1.0;
    //   normal = normalize(TBN * normal);
    // }
  }
  gNormal = float32x3_to_oct(normalize(normal));
  //gNormal.xyz = normalize(normal);
#if VISUALIZE_MAPS
  //gNormal = float32x3_to_oct(normalize(normal) * .5 + .5);
  gNormal.xyz = (normalize(normal) * .5 + .5);
#endif
  vec4 color = texture(diffuse, vTexCoord).rgba;
  if (color.a <= .01)
  {
    discard;
  }
  gAlbedoSpec.rgb = color.rgb;
  gAlbedoSpec.a = 0.0;
  if (hasSpecular)
  {
    gAlbedoSpec.a = texture(specular, vTexCoord).r;
  }
  gShininess.r = shininess;
}