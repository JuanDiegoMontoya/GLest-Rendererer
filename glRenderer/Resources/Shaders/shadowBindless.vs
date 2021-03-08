#version 460 core

layout (location = 0) in vec3 aPos;

struct ShadowUniformData
{
  mat4 modelLightMatrix;
};

layout (binding = 0, std430) readonly buffer Uniforms
{
  ShadowUniformData uniforms[];  
};

void main()
{
  gl_Position = uniforms[gl_DrawID].modelLightMatrix * vec4(aPos, 1.0);
}