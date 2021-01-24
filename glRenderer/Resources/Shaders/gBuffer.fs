#version 460 core

#define VISUALIZE_MAPS 0

struct Material
{
  sampler2D diffuse;
  sampler2D specular;
};

// texture buffers
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gAlbedoSpec;
layout (location = 2) out vec4 gNormal;

layout (location = 0) in vec3 vNormal;
layout (location = 1) in vec3 vPos;
layout (location = 2) in vec2 vTexCoord;

layout (location = 3) uniform Material u_object;

void main()
{
  gPosition = vec4(vPos, 1.0);
  gNormal = vec4(normalize(vNormal), 1.0);
#if VISUALIZE_MAPS
  gPosition = normalize(vPos) * .5 + .5;
  gNormal = normalize(vNormal) * .5 + .5;
#endif
  //gAlbedoSpec.rgb = texture(u_object.diffuse, vTexCoord).rgb;
  //gAlbedoSpec.a = texture(u_object.specular, vTexCoord).r;
  gAlbedoSpec = vec4(.5, .7, .7, .1);
}