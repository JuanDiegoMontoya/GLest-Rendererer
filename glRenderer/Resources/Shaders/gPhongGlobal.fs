#version 460 core

#define SHININESS 64.0f
#define SPECULAR_STRENGTH 5

struct DirLight
{
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;

  vec3 direction;
};

layout (location = 0) in vec2 vTexCoord;

layout (location = 0) uniform vec3 u_viewPos;
layout (location = 1) uniform sampler2D gPosition;
layout (location = 2) uniform sampler2D gNormal;
layout (location = 3) uniform sampler2D gAlbedoSpec;
layout (location = 4) uniform DirLight u_globalLight;

layout (location = 0) out vec4 fragColor;

void main()
{
  vec3 albedo = texture(gAlbedoSpec, vTexCoord).rgb;
  float specular = texture(gAlbedoSpec, vTexCoord).a;
  vec3 vPos = texture(gPosition, vTexCoord).xyz;
  vec3 vNormal = texture(gNormal, vTexCoord).xyz;

  if (vNormal == vec3(0))
  {
    discard;
  }

  vec3 viewDir = normalize(u_viewPos - vPos);
  
  vec3 lightDir = normalize(-u_globalLight.direction);
  // diffuse shading
  float diff = max(dot(vNormal, lightDir), 0.0);
  
  // specular shading
  float spec = 0;
  vec3 reflectDir = reflect(-lightDir, vNormal);
  spec = pow(max(dot(viewDir, reflectDir), 0.0), SHININESS) * SPECULAR_STRENGTH;
  
  // combine results
  vec3 ambient = u_globalLight.ambient * albedo;
  vec3 diffuse = u_globalLight.diffuse * diff * albedo;
  vec3 specu = u_globalLight.specular * spec * specular;
  fragColor = vec4((ambient + diffuse + specu), 1.0);// * .0001 + spec;
}