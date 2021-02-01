#version 460 core
#include "common.h"

#define SPECULAR_STRENGTH 5

struct DirLight
{
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;

  vec3 direction;
};

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform sampler2D gNormal;
layout (location = 1) uniform sampler2D gAlbedoSpec;
layout (location = 2) uniform sampler2D gShininess;
layout (location = 3) uniform sampler2D gDepth;
layout (location = 4) uniform sampler2D shadowDepth;
layout (location = 5) uniform vec3 u_viewPos;
layout (location = 6) uniform mat4 u_lightMatrix;
layout (location = 7) uniform mat4 u_invViewProj;
layout (location = 8) uniform DirLight u_globalLight;

layout (location = 0) out vec4 fragColor;

float Shadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir)
{
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  if(projCoords.z > 1.0) return 0.0;
  projCoords = projCoords * 0.5 + 0.5;
  float closestDepth = texture(shadowDepth, projCoords.xy).r;

  float currentDepth = projCoords.z;
  float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
  float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

  // apply some filtering

  return shadow;
}

void main()
{
  vec2 texSize = textureSize(gNormal, 0);
  vec3 albedo = texture(gAlbedoSpec, vTexCoord).rgb;
  float specular = texture(gAlbedoSpec, vTexCoord).a;
  vec3 vPos = WorldPosFromDepth(texture(gDepth, vTexCoord).r, texSize, u_invViewProj);
  vec3 vNormal = oct_to_float32x3(texture(gNormal, vTexCoord).xy);
  //vec3 vNormal = texture(gNormal, vTexCoord).xyz;
  float shininess = texture(gShininess, vTexCoord).r;
  vec4 lightSpacePos = u_lightMatrix * vec4(vPos, 1.0);

  if (vNormal == vec3(0))
  {
    discard;
  }

  vec3 viewDir = normalize(u_viewPos - vPos);
  vec3 lightDir = normalize(-u_globalLight.direction);

  float diff = max(dot(vNormal, lightDir), 0.0);
  
  float spec = 0;
  vec3 reflectDir = reflect(-lightDir, vNormal);
  spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess) * SPECULAR_STRENGTH;
  
  vec3 ambient = u_globalLight.ambient * albedo;
  vec3 diffuse = u_globalLight.diffuse * diff * albedo;
  vec3 specu = u_globalLight.specular * spec * specular;

  float shadow = Shadow(lightSpacePos, vNormal, lightDir);
  fragColor = vec4((ambient + (1.0 - shadow) * (diffuse + specu)), 1.0);
  //fragColor = fragColor * .0001 + vec4(shadow); // view shadow
}