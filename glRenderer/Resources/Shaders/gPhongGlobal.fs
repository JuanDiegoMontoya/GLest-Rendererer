#version 460 core
#include "common.h"

#define SPECULAR_STRENGTH 5

#define SHADOW_METHOD_PCF 0
#define SHADOW_METHOD_VSM 1
#define SHADOW_METHOD_ESM 2
#define SHADOW_METHOD_MSM 3

struct DirLight
{
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;

  vec3 direction;
};

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D gNormal;
layout (location = 1, binding = 1) uniform sampler2D gAlbedoSpec;
layout (location = 2, binding = 2) uniform sampler2D gShininess;
layout (location = 3, binding = 3) uniform sampler2D gDepth;
layout (location = 4, binding = 4) uniform sampler2D shadowMap; // PCF
layout (location = 5, binding = 5) uniform sampler2D filteredShadow; // ESM or VSM
layout (location = 6) uniform vec3 u_viewPos;
layout (location = 7) uniform mat4 u_lightMatrix;
layout (location = 8) uniform mat4 u_invViewProj;
layout (location = 9) uniform float u_lightBleedFix = .9;
layout (location = 10) uniform int u_shadowMethod = SHADOW_METHOD_ESM;
layout (location = 11) uniform float u_C;
layout (location = 12) uniform DirLight u_globalLight;

layout (location = 0) out vec4 fragColor;

vec3 ShadowTexCoord(vec4 lightSpacePos)
{
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  if(projCoords.z > 1.0) return vec3(1.0);
  projCoords = projCoords * 0.5 + 0.5;
  return projCoords.xyz;
}

const float u_minVariance = .000001; // for numeric precision
float linstep(float min, float max, float v)
{
  return clamp((v - min) / (max - min), 0.0, 1.0);
}
float ReduceLightBleeding(float p_max, float amount)
{
  // Remove the [0, Amount] tail and linearly rescale (Amount, 1].
  return linstep(amount, 1.0, p_max);
}
// use Chebyshev inequality to compute fraction of texel that is shadowed
float Chebyshev(vec2 moments, float t)
{
  // standard shadow map test
  float p = (t <= moments.x) ? 1.0 : 0.0;
  float variance = moments.y - (moments.x * moments.x);
  variance = max(variance, u_minVariance);
  float d = moments.x - t;
  float p_max = ReduceLightBleeding(variance / (variance + d * d), u_lightBleedFix);
  return max(p, p_max);
}

float ShadowVSM(vec4 lightSpacePos)
{
  vec3 LightTexCoord = ShadowTexCoord(lightSpacePos);
  vec2 moments = texture(filteredShadow, LightTexCoord.xy).xy;
  return Chebyshev(moments, LightTexCoord.z);
}

float ShadowESM(vec4 lightSpacePos)
{
  vec3 LightTexCoord = ShadowTexCoord(lightSpacePos);
  float lightDepth = texture(filteredShadow, LightTexCoord.xy).x;
  float eyeDepth = LightTexCoord.z;
  float shadowFacktor = lightDepth * exp(-u_C * eyeDepth);
  return clamp(shadowFacktor, 0.0, 1.0);
}

float ShadowMSM(vec4 lightSpacePos)
{
  return 1.0;
}

float Shadow(vec4 lightSpacePos)
{
  switch (u_shadowMethod)
  {
    case SHADOW_METHOD_ESM: return ShadowESM(lightSpacePos);
    case SHADOW_METHOD_VSM: return ShadowVSM(lightSpacePos);
    default: return 1.0;
  }
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
  
  float spec = 0.0;
  vec3 reflectDir = reflect(-lightDir, vNormal);
  spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess) * SPECULAR_STRENGTH;
  
  vec3 ambient = u_globalLight.ambient * albedo;
  vec3 diffuse = u_globalLight.diffuse * diff * albedo;
  vec3 specu = u_globalLight.specular * spec * specular;

  float shadow = 0.0;
  if (diff > 0.0) // only shadow light-facing pixels
  {
    shadow = Shadow(lightSpacePos);
  }
  fragColor = vec4((ambient + (shadow) * (diffuse + step(.005, shadow) * specu)), 1.0);
  //fragColor = fragColor * .0001 + vec4(shadow); // view shadow
}