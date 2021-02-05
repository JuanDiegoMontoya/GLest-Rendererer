#version 460 core
#define KERNEL_RADIUS 3
#define FORMAT RG32f
#define VECTOR vec4

layout (location = 0) uniform bool u_horizontal = false;
layout (location = 1) uniform sampler2D u_inTex;
layout (location = 2, FORMAT) uniform image2D u_outTex;
layout (location = 3) uniform ivec2 u_texSize;

#if KERNEL_RADIUS == 6
const float weights[] = { 0.22528, 0.192187, 0.119319, 0.053904, 0.017716, 0.004235 }; // 11x11
#elif KERNEL_RADIUS == 5
const float weights[] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 }; // 9x9
#elif KERNEL_RADIUS == 4
const float weights[] = { 0.235624, 0.201012, 0.124798, 0.056379 }; // 7x7
#elif KERNEL_RADIUS == 3
const float weights[] = { 0.265569, 0.226558, 0.140658 }; // 5x5
#elif KERNEL_RADIUS == 2
const float weights[] = { 0.369521, 0.31524 }; // 3x3
#elif KERNEL_RADIUS == 1
const float weights[] = { 1.0 }; // 1x1 (lol)
#endif

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
  const ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  if (any(lessThan(texel - KERNEL_RADIUS + 1, ivec2(0))) || any(greaterThanEqual(texel + KERNEL_RADIUS - 1, u_texSize)))
  {
    return;
  }

  VECTOR color = texelFetch(u_inTex, texel, 0).rgba * weights[0];
  if (u_horizontal)
  {
    for (int i = 1; i < KERNEL_RADIUS; i++)
    {
      color += texelFetch(u_inTex, texel + ivec2(i, 0.0), 0).rgba * weights[i];
      color += texelFetch(u_inTex, texel - ivec2(i, 0.0), 0).rgba * weights[i];
    }
  }
  else
  {
    for (int i = 1; i < KERNEL_RADIUS; i++)
    {
      color += texelFetch(u_inTex, texel + ivec2(0.0, i), 0).rgba * weights[i];
      color += texelFetch(u_inTex, texel - ivec2(0.0, i), 0).rgba * weights[i];
    }
  }

  imageStore(u_outTex, texel, vec4(color));
}