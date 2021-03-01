#version 460 core
#include "common.h"

#define SPECULAR_STRENGTH 5

#define SHADOW_METHOD_PCF 0
#define SHADOW_METHOD_VSM 1
#define SHADOW_METHOD_ESM 2
#define SHADOW_METHOD_MSM 3

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
layout (location = 12) uniform vec3 u_globalLight_ambient;
layout (location = 13) uniform vec3 u_globalLight_diffuse;
layout (location = 14) uniform vec3 u_globalLight_specular;
layout (location = 15) uniform vec3 u_globalLight_direction;

layout (location = 0) out vec4 fragColor;

vec3 ShadowTexCoord(vec4 lightSpacePos)
{
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
  if(projCoords.z > 1.0) return vec3(1.0);
  projCoords = projCoords * 0.5 + 0.5;
  return projCoords.xyz;
}

const float u_minVariance = .000001; // compensate for numeric precision
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

float compute_msm_shadow_intensity(vec4 b, float fragment_depth, float depthBias, float momentBias)
{
  // OpenGL 4 only - fma has higher precision:
  // float l32_d22 = fma(-b.x, b.y, b.z); // a * b + c
  // float d22 = fma(-b.x, b.x, b.y);     // a * b + c
  // float squared_depth_variance = fma(-b.x, b.y, b.z); // a * b + c
  b = mix(b, vec4(0.5), momentBias);
  fragment_depth -= depthBias;

  float l32_d22 = -b.x * b.y + b.z;
  float d22 = -b.x *  b.x + b.y;
  float squared_depth_variance = -b.x * b.y + b.z;
  
  float d33_d22 = dot(vec2(squared_depth_variance, -l32_d22), vec2(d22, l32_d22));
  float inv_d22 = 1.0 - d22;
  float l32 = l32_d22 * inv_d22;

  float z_zero = fragment_depth;
  vec3 c = vec3(1.0, z_zero - b.x, z_zero * z_zero);
  c.z -= b.y + l32 * c.y;
  c.y *= inv_d22;
  c.z *= d22 / d33_d22;
  c.y -= l32 * c.z;
  c.x -= dot(c.yz, b.xy);
  
  float inv_c2 = 1.0 / c.z;
  float p = c.y * inv_c2;
  float q = c.x * inv_c2;
  float r = sqrt((p * p * 0.25) - q);

  float z_one = -p * 0.5 - r;
  float z_two = -p * 0.5 + r;
  
  vec4 switch_msm;
  if (z_two < z_zero)
  {
    switch_msm = vec4(z_one, z_zero, 1.0, 1.0);
  }
  else
  {
    if (z_one < z_zero)
    {
      switch_msm = vec4(z_zero, z_one, 0.0, 1.0);
    }
    else
    {
      switch_msm = vec4(0.0);
    }
  }
  
  float quotient = (switch_msm.x * z_two - b.x * (switch_msm.x + z_two + b.y)) / ((z_two - switch_msm.y) * (z_zero - z_one));
  return 1.0 - clamp(switch_msm.y + switch_msm.z * quotient, 0.0, 1.0);
}

vec3 cholesky(float m11, float m12, float m13, float m22, float m23, float m33, float z1, float z2, float z3)
{
  float a = sqrt(m11);
  float b = m12 / a;
  float c = m13 / a;
  float d = sqrt(m22 - b * b);
  float e = (m23 - b * c) / d;
  float f = sqrt(m33 - c * c - e * e);

  float c1hat = z1 / a;
  float c2hat = (z2 - b * c1hat) / d;
  float c3hat = (z3 - c * c1hat - e * c2hat) / f;

  float c3 = c3hat / f;
  float c2 = (c2hat - e * c3) / d;
  float c1 = (c1hat - b * c2hat - c * c3hat) / a;

  return vec3(c1, c2, c3);
}

vec2 findQuadraticRoots(float a, float b, float c)
{
  float discriminant = sqrt(b * b - 4 * a * c);
  float r1 = (-b + discriminant) / (2 * a);
  float r2 = (-b - discriminant) / (2 * a);
  return vec2(r1, r2);
}

float msmBias = 3e-5;
float ShadowMSM(vec4 lightSpacePos, float cosTheta)
{
  float depthBias = .005 * tan(acos(cosTheta)) ;
  depthBias = clamp(depthBias, 0.0, 0.1);

  vec3 LightTexCoord = ShadowTexCoord(lightSpacePos);
  vec4 moments = texture(filteredShadow, LightTexCoord.xy).xyzw;

  mat4 magic = mat4(0.2227744146, 0.1549679261, 0.1451988946, 0.163127443,
                    0.0771972861, 0.1394629426, 0.2120202157, 0.2591432266,
                    0.7926986636, 0.7963415838, 0.7258694464, 0.6539092497,
                    0.0319417555,-0.1722823173,-0.2758014811,-0.3376131734);
  moments.x -= 0.035955884801;
  moments = magic * moments;
  return compute_msm_shadow_intensity(moments, LightTexCoord.z, depthBias * 0.15, msmBias);
  // float a = msmBias;
  // float zf = LightTexCoord.z;
  // vec4 b = moments;
  // vec4 bPrime = (1.0 - a) * b + a * vec4(0.5);
  // vec3 c = cholesky(1.0, b.x, b.y, b.y, b.z, b.w, 1, zf, zf * zf);
  // vec2 roots = findQuadraticRoots(c.x, c.y, c.z);
  // float z2 = roots.x;
  // float z3 = roots.y;
  // if (zf <= z2)
  // {
  //   return 0.0;
  // }
  // if (zf <= z3)
  // {
  //   return (zf * z3 - bPrime.x * (zf + z3) + bPrime.y) / ((z3 - z2) * (zf - z2));
  // }

  // return 1.0 - (z2 * z3 - bPrime.x * (z2 + z3) + bPrime.y) / ((zf - z2) * (zf - z3));
}

float Shadow(vec4 lightSpacePos, float cosTheta)
{
  switch (u_shadowMethod)
  {
    case SHADOW_METHOD_ESM: return ShadowESM(lightSpacePos);
    case SHADOW_METHOD_VSM: return ShadowVSM(lightSpacePos);
    case SHADOW_METHOD_MSM: return ShadowMSM(lightSpacePos, cosTheta);
    default: return 1.0;
  }
}

void main()
{
  vec2 texSize = textureSize(gNormal, 0);
  vec4 albedoSpec = texture(gAlbedoSpec, vTexCoord).rgba;
  vec3 albedo = albedoSpec.rgb;
  float specular = albedoSpec.a;
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
  vec3 lightDir = normalize(-u_globalLight_direction);

  float diff = max(dot(vNormal, lightDir), 0.0);
  
  float spec = 0.0;
  vec3 reflectDir = reflect(-lightDir, vNormal);
  spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess) * SPECULAR_STRENGTH;
  
  vec3 ambient = u_globalLight_ambient * albedo;
  vec3 diffuse = u_globalLight_diffuse * diff * albedo;
  vec3 specu = u_globalLight_specular * spec * specular;

  float shadow = 0.0;
  if (diff > 0.0) // only shadow light-facing pixels
  {
    shadow = Shadow(lightSpacePos, clamp(dot(vNormal, lightDir), -1.0, 1.0));
  }
  fragColor = vec4((ambient + (shadow) * (diffuse + step(.005, shadow) * specu)), 1.0);
  //fragColor = fragColor * .0001 + vec4(shadow); // view shadow
}