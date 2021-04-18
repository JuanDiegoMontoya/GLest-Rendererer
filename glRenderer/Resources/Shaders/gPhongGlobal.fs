#version 460 core
#include "common.h"
#include "pbr_common.h"

#define SHADOW_METHOD_PCF 0
#define SHADOW_METHOD_VSM 1
#define SHADOW_METHOD_ESM 2
#define SHADOW_METHOD_MSM 3

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D gNormal;
layout (location = 1, binding = 1) uniform sampler2D gAlbedo;
layout (location = 2, binding = 2) uniform sampler2D gRMA;
layout (location = 3, binding = 3) uniform sampler2D gDepth;
layout (location = 4, binding = 4) uniform sampler2D ambientOcclusionTexture; // PCF, raw shadowmap
layout (location = 5, binding = 5) uniform sampler2D filteredShadow; // ESM or VSM
layout (location = 6, binding = 6) uniform sampler2D env_irradiance;
layout (location = 7, binding = 7) uniform sampler2D env_radiance;
layout (location = 8) uniform ivec2 u_screenSize;
layout (location = 9) uniform uint u_samples;
layout (location = 10) uniform vec3 u_viewPos;
layout (location = 11) uniform mat4 u_lightMatrix;
layout (location = 12) uniform mat4 u_invViewProj;
layout (location = 13) uniform float u_lightBleedFix = .9;
layout (location = 14) uniform int u_shadowMethod = SHADOW_METHOD_ESM;
layout (location = 15) uniform float u_C;
layout (location = 16) uniform vec3 u_globalLight_diffuse;
layout (location = 17) uniform vec3 u_globalLight_direction;
layout (location = 18) uniform float msmBias = 3e-5;

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

// find solutions to linear system conforming to a certain specification
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

float computeMSM(vec4 moments, float fragmentDepth, float depthBias, float momentBias)
{
  vec4 b = moments * (1.0 - momentBias) + momentBias * vec4(0.5, 0.5, 0.5, 0.5);
  vec3 z = vec3(fragmentDepth - depthBias, 0.0, 0.0);

  float L32D22 = -b[0] * b[1] + b[2];
  float D22    = -b[0] * b[0] + b[1];
  float SDV    = -b[1] * b[1] + b[3];
  float D33D22 = dot( vec2(SDV, -L32D22), vec2(D22, L32D22) );
  float InvD22 = 1.0 / D22;
  float L32    = L32D22 * InvD22;
  
  // high IQ cholesky
  vec3 c = vec3(1.0f, z[0], z[0] * z[0]);
  c[1] -= b.x;
  c[2] -= b.y + L32 * c[1];
  c[1] *= InvD22;
  c[2] *= D22 / D33D22;
  c[1] -= L32 * c[2];
  c[0] -= dot(c.yz, b.xy);
  //vec3 c = cholesky(1.0, b.x, b.y, b.y, b.z, b.w, 1.0, z.x, z.x * z.x);
  
  // solve quadratic
  float p = c[1] / c[2];
  float q = c[0] / c[2];
  float D = ((p * p) / 4.0) - q;
  float r = sqrt(D);
  z[1] = -(p / 2.0f) - r;
  z[2] = -(p / 2.0f) + r;
  
  // swizzle
  vec4 sv = vec4(0.0);
  if (z[2] < z[0])
  {
    sv = vec4(z[1], z[0], 1.0, 1.0);
  }
  else if (z[1] < z[0])
  {
    sv = vec4(z[0], z[1], 0.0f, 1.0);
  }
  
  float quotient = (sv[0] * z[2] - b[0] * (sv[0] + z[2]) + b[1]) / ((z[2] - sv[1]) * (z[0] - z[1]));
  
  return 1.0 - clamp(sv[2] + sv[3] * quotient, 0.0, 1.0);
}

float ShadowMSM(vec4 lightSpacePos, float cosTheta)
{
  float depthBias = .005 * tan(acos(cosTheta));
  depthBias = clamp(depthBias, 0.0, 0.1);
  vec3 LightTexCoord = ShadowTexCoord(lightSpacePos);
  vec4 moments = texture(filteredShadow, LightTexCoord.xy).xyzw;
  mat4 magic = mat4(0.2227744146, 0.1549679261, 0.1451988946, 0.163127443,
                    0.0771972861, 0.1394629426, 0.2120202157, 0.2591432266,
                    0.7926986636, 0.7963415838, 0.7258694464, 0.6539092497,
                    0.0319417555,-0.1722823173,-0.2758014811,-0.3376131734);
  moments.x -= 0.035955884801;
  moments = magic * moments; // undo original curve
  return computeMSM(moments, LightTexCoord.z, depthBias * 0.15, msmBias);
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

float CalcLOD(uint samples, vec3 N, vec3 H, float roughness)
{
  float dist = D_GGX(N, H, roughness);
  return 0.5 * (log2(float(u_screenSize.x * u_screenSize.y) / samples) - log2(dist));
}

vec3 ComputeSpecularRadiance(vec3 N, vec3 V, vec3 F0, float roughness)
{
  vec3 up = abs(N.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
  vec3 tanX = normalize(cross(up, N));
  vec3 tanY = cross(N, tanX);

  float NoV = abs(dot(N, V));

  vec3 accumColor = vec3(0.0);
  const uint samples = u_samples;
  for (uint i = 0; i < samples; i++)
  {
    vec2 Xi = Hammersley(i, samples);
    vec3 H = ImportanceSampleGGX(Xi, N, roughness);
    vec3 L = normalize(-reflect(V, H));
    //vec3 L  = normalize(2.0 * dot(V, H) * H - V);
    
    float NoL = abs(dot(N, L));
    float NoH = abs(dot(N, H));
    float VoH = abs(dot(V, H));
    float lod = CalcLOD(samples, N, H, roughness);

    vec3 F_ = fresnelSchlick(VoH, F0);
    float G_ = G_Smith(N, V, L, roughness);
    vec2 uuv = NormToEquirectangularUV(L);
    vec3 LColor = textureLod(env_radiance, uuv, lod).rgb;

    accumColor += F_ * G_ * LColor * VoH / max((NoH * NoV), 0.001);
  }

  return accumColor / float(samples);
}

void main()
{
  float depth = texture(gDepth, vTexCoord).r;

  if (depth == 1.0)
  {
    discard;
  }

  vec3 albedo = texture(gAlbedo, vTexCoord).rgb;
  vec3 vPos = WorldPosFromDepth(depth, u_screenSize, u_invViewProj);
  vec2 octNormal = texture(gNormal, vTexCoord).xy;
  vec3 vNormal = oct_to_float32x3(octNormal);
  //vec3 vNormal = texture(gNormal, vTexCoord).xyz;
  vec4 RMA = texture(gRMA, vTexCoord);
  float roughness = clamp(RMA[0], 0.01, 1.0);
  float metalness = RMA[1];
  float ambientOcclusion = RMA[2]; // material AO
  float ambientOcclusion2 = texture(ambientOcclusionTexture, vTexCoord).r; // SSAO
  ambientOcclusion *= ambientOcclusion2;
  vec4 lightSpacePos = u_lightMatrix * vec4(vPos, 1.0);

  vec3 N = normalize(vNormal);
  vec3 V = normalize(u_viewPos - vPos);
  vec3 F0 = mix(vec3(0.04), albedo, metalness);
  float NoV = max(dot(N, V), 0.0);
  F0 = clamp(F0, vec3(0.01), vec3(0.99));
  vec3 kS = fresnelSchlickRoughness(NoV, F0, roughness);
  vec3 kD = 1.0 - kS;
  kD *= 1.0 - metalness;
  vec3 irradiance = texture(env_irradiance, NormToEquirectangularUV(N)).rgb;
  vec3 envDiffuse = irradiance * albedo;
  vec3 envSpecular = vec3(0.0);
  if (metalness > 0.0 || roughness < 1.0)
  {
    envSpecular = ComputeSpecularRadiance(N, V, F0, roughness) / M_PI;
  }

  vec3 env = (kD * envDiffuse + envSpecular) * ambientOcclusion;
  //env = env * .0001 + envSpecular;

  // directional light
  {
    vec3 L = normalize(-u_globalLight_direction);
    vec3 H = normalize(V + L);
    float NoL = abs(dot(N, L));
    vec3 radiance = u_globalLight_diffuse.rgb;
    float NDF = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metalness;

    // cook-torrance brdf
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * abs(dot(N, V)) * abs(dot(N, L));
    vec3 specular = numerator / max(denominator, 0.001);

    float cosTheta = abs(dot(N, L));
    vec3 local = (kD * albedo / M_PI + specular) * radiance * cosTheta;

    float shadow = 0.0;
    if (NoL > 0.0) // only shadow light-facing pixels
    {
      shadow = Shadow(lightSpacePos, clamp(dot(N, L), -1.0, 1.0));
    }
    //fragColor = vec4((shadow * (diffuse + step(.005, shadow) * specular)), 1.0);
    fragColor = vec4((shadow * local), 1.0);
  }

  fragColor.rgb += env;
  //fragColor = fragColor * .0001 + vec4(irradiance, 1.0);
  //fragColor = fragColor * .0001 + vec4(shadow); // view shadow
}