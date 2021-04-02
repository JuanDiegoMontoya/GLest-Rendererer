#version 460 core
#include "common.h"

#define SPECULAR_STRENGTH 1
#define M_PI 3.1415926536

#define PBR 1

struct PointLight
{
  vec4 diffuse;

  vec4 position;
  float linear;
  float quadratic;
  float radiusSquared;
  float _padding;
};

layout (location = 0) in flat PointLight vLight;

layout (location = 2) uniform sampler2D gNormal;
layout (location = 3) uniform sampler2D gAlbedo;
layout (location = 4) uniform sampler2D gRMA;
layout (location = 5) uniform sampler2D gDepth;
layout (location = 6) uniform vec3 u_viewPos;
layout (location = 7) uniform mat4 u_invViewProj;

layout (location = 0) out vec4 fragColor;

vec3 CalcLocalColor(PointLight light);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 viewDir);

float D_GGX(vec3 N, vec3 H, float roughness)
{
  float a = roughness * roughness; // makes it look better apparently
  float cosTheta = max(dot(N, H), 0.0);
  float tmp = pow(cosTheta * ((a * a) - 1.0) + 1.0, 2.0);
  return (a * a) / (M_PI * tmp);
}

float G_SchlickGGX(vec3 N, vec3 V, float roughness)
{
  float k = pow(roughness + 1.0, 2.0) / 8.0;
  float cosTheta = max(dot(N, V), 0.0);
  return cosTheta / (cosTheta * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness)
{
  return G_SchlickGGX(N, V, roughness) * G_SchlickGGX(N, L, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
  return F0 + (1.0 - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

float GetAttenuation(float distance, float linearFalloff, float quadraticFalloff)
{
  return 1.0 / (linearFalloff * distance + quadraticFalloff * (distance * distance));
}

void main()
{
  vec2 texSize = textureSize(gNormal, 0);
  vec2 texCoord = gl_FragCoord.xy / texSize;
  vec3 vNormal = oct_to_float32x3(texture(gNormal, texCoord).xy);
  vec3 vPos = WorldPosFromDepth(texture(gDepth, texCoord).r, texSize, u_invViewProj);
  //vec3 vNormal = texture(gNormal, texCoord).xyz;

  float distanceToLightSquared = dot(vPos - vLight.position.xyz, vPos - vLight.position.xyz);
  if (vNormal == vec3(0) || distanceToLightSquared > vLight.radiusSquared)
  {
    discard;
  }

  vec3 albedo = texture(gAlbedo, texCoord).rgb;
  float attenuation = GetAttenuation(sqrt(distanceToLightSquared), vLight.linear, vLight.quadratic);

  vec4 RMA = texture(gRMA, texCoord);
  float roughness = RMA[0];
  float metalness = RMA[1];
  float ambientOcclusion = RMA[2];

  vec3 N = normalize(vNormal);
  vec3 V = normalize(u_viewPos - vPos);
  vec3 L = normalize(vLight.position.xyz - vPos);
  vec3 H = normalize(V + L);
  vec3 F0 = mix(vec3(0.04), albedo, metalness);
  vec3 radiance = vLight.diffuse.rgb * attenuation;

  float NDF = D_GGX(N, H, roughness);
  float G = G_Smith(N, V, L, roughness);
  vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

  vec3 kS = F;
  vec3 kD = vec3(1.0) - kS;
  kD *= 1.0 - metalness;

  // cook-torrance brdf
  vec3 numerator = NDF * G * F;
  float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
  vec3 specular = numerator / max(denominator, 0.001);

  float cosTheta = max(dot(N, L), 0.0);
  vec3 local = (kD * albedo / M_PI + specular) * radiance * cosTheta;
  fragColor = vec4(local, 0.0);

#if !PBR
  vec3 local = CalcPointLight(vLight, N, V);
  local *= attenuation;

  //fragColor = vec4(local, 0.0) * smoothstep((vLight.radiusSquared), .4 * (vLight.radiusSquared), (distanceToLightSquared));
  fragColor = vec4(local, 0.0);
#endif
}

// vec3 CalcLocalColor(PointLight light, vec3 lightDir, vec3 normal, vec3 viewDir)
// {
//   float diff = max(dot(normal, lightDir), 0.0);
  
//   float spec = 0;
//   vec3 reflectDir = reflect(-lightDir, normal);
//   spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0) * SPECULAR_STRENGTH;
  
//   vec3 diffuse = (light.diffuse.xyz)  * diff * albedo;
//   vec3 specu = light.diffuse.xyz * spec * (1.0 - roughness);
//   return (diffuse + specu);
// }


// vec3 CalcPointLight(PointLight light, vec3 normal, vec3 viewDir)
// {
//   vec3 lightDir = normalize(light.position.xyz - vPos);
//   vec3 local = CalcLocalColor(light, lightDir, normal, viewDir);
//   return local;
// }