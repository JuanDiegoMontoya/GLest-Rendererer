#version 460 core
#include "common.h"

#define SHININESS 64.0f
#define SPECULAR_STRENGTH 1

struct PointLight
{
  vec4 diffuse;
  vec4 specular;

  vec4 position;
  float linear;
  float quadratic;
  float radiusSquared;
  float _padding;
};

layout (std430, binding = 0) readonly buffer lit
{
  PointLight lights[];
};

layout (location = 0) in vec3 vLightPos;
layout (location = 1) in flat int vInstanceID;

layout (location = 2) uniform vec3 u_viewPos;
layout (location = 4) uniform sampler2D gPosition;
layout (location = 5) uniform sampler2D gNormal;
layout (location = 6) uniform sampler2D gAlbedoSpec;
layout (location = 7) uniform mat4 u_invProj;
layout (location = 8) uniform mat4 u_invView;
//layout (location = 9) uniform PointSpotLight u_light;

layout (location = 0) out vec4 fragColor;

vec3 CalcLocalColor(PointLight light);
float CalcAttenuation(PointLight light);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 viewDir);
vec3 WorldPosFromDepth(float depth);

vec3 albedo;
float specular;
vec3 vPos;
void main()
{
  vec2 texCoord = gl_FragCoord.xy / textureSize(gPosition, 0).xy;
  albedo = texture(gAlbedoSpec, texCoord).rgb;
  specular = texture(gAlbedoSpec, texCoord).a;
  vPos = texture(gPosition, texCoord).xyz;
  vec3 vNormal = oct_to_float32x3(texture(gNormal, texCoord).xy);

  float distanceToLightSquared = dot(vPos - lights[vInstanceID].position.xyz, vPos - lights[vInstanceID].position.xyz);
  if (vNormal == vec3(0) || distanceToLightSquared > lights[vInstanceID].radiusSquared)
  {
    fragColor = vec4(0);
    return;
  }

  vec3 pixelPos = WorldPosFromDepth(0.0);
  float pixelDistanceToLightSquared = dot(pixelPos - lights[vInstanceID].position.xyz, pixelPos - lights[vInstanceID].position.xyz);
  if ((gl_FrontFacing == true && pixelDistanceToLightSquared < lights[vInstanceID].radiusSquared) || 
    gl_FrontFacing == false && pixelDistanceToLightSquared >= lights[vInstanceID].radiusSquared)
  {
    fragColor = vec4(0);
    return;
  }

  vec3 normal = normalize(vNormal);
  vec3 viewDir = normalize(u_viewPos - vPos);
  vec3 local = CalcPointLight(lights[vInstanceID], normal, viewDir);

  fragColor = vec4(local, 0.0) * smoothstep(lights[vInstanceID].radiusSquared, .7 * lights[vInstanceID].radiusSquared, distanceToLightSquared);
}

vec3 CalcLocalColor(PointLight light, vec3 lightDir, vec3 normal, vec3 viewDir)
{
  float diff = max(dot(normal, lightDir), 0.0);
  
  float spec = 0;
  vec3 reflectDir = reflect(-lightDir, normal);
  spec = pow(max(dot(viewDir, reflectDir), 0.0), SHININESS) * SPECULAR_STRENGTH;
  
  vec3 diffuse  = (light.diffuse.xyz)  * diff * albedo;
  vec3 specu = light.specular.xyz * spec * specular;
  return (diffuse + specu);
}

float CalcAttenuation(PointLight light)
{
  float distance = length(light.position.xyz - vPos);
  return 1.0 / (light.linear * distance + light.quadratic * (distance * distance));
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 viewDir)
{
  vec3 lightDir = normalize(light.position.xyz - vPos);
  vec3 local = CalcLocalColor(light, lightDir, normal, viewDir);
  local *= CalcAttenuation(light);
  return local;
}

vec3 WorldPosFromDepth(float depth)
{
  float z = depth * 2.0 - 1.0; // [0, 1] -> [1, 1]
  vec2 normalized = gl_FragCoord.xy / textureSize(gPosition, 0); // [0.5, u_viewPortSize] -> [0, 1]
  vec4 clipSpacePosition = vec4(normalized * 2.0 - 1.0, z, 1.0); // [0, 1] -> [-1, 1]
  vec4 viewSpacePosition = u_invProj * clipSpacePosition; // undo projection

  // perspective division
  viewSpacePosition /= viewSpacePosition.w;

  // undo view
  vec4 worldSpacePosition = u_invView * viewSpacePosition;

  return worldSpacePosition.xyz;
}