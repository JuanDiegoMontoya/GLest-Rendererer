#version 460 core

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D u_tex;

layout (location = 0) out vec4 outMoments;

void main()
{
  mat4 magic = mat4(-2.07224649,   13.7948857237,  0.105877704,   9.7924062118,
                     32.23703778,  -59.4683975703, -1.9077466311,-33.7652110555,
                    -68.571074599,  82.0359750338,  9.3496555107, 47.9456096605,
                     39.3703274134,-35.364903257,  -6.6543490743,-23.9728048165);
  float depth = texture(u_tex, vTexCoord).r;
  outMoments.xyzw = vec4(depth, depth * depth, depth * depth * depth, depth * depth * depth * depth);
  outMoments = magic * outMoments;
  outMoments.x += 0.035955884801;
}