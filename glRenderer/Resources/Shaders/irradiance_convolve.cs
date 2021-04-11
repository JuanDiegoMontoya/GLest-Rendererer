#version 460 core
layout (location = 0) uniform sampler2D u_environment;
layout (location = 1, RGBA16f) uniform image2D u_outTex;

const float PI = 3.14159265359;

vec3 uv2normal(vec2 uv)
{
  vec3 N =
   {
    cos(2 * PI * (0.5 - uv.x)) * sin(PI * uv.y),
    sin(2 * PI * (0.5 - uv.x)) * sin(PI * uv.y),
    cos(PI * uv.y)
  };
  return N;
}

vec2 normal2uv(vec3 normal)
{
  // vec2 uv =
  // {
  //   0.5 - atan(normal.y, normal.x) / (2 * PI),
  //   acos(normal.z) / PI
  // };
  // return uv;
  
  float phi = acos(-normal.y);
  float theta = atan(normal.z, normal.x) + PI; // or normal.xz?
  vec2 uv = vec2(theta / (PI * 2.0), phi / PI);
  return uv;

  // const vec2 invAtan = vec2(0.1591f, 0.3183f);
  // vec2 uv = vec2(atan(normal.z, normal.x), asin(normal.y));
  // uv *= invAtan;
  // uv += 0.5;
  // return uv;
}

float map(float val, float r1s, float r1e, float r2s, float r2e)
{
  return (val - r1s) / (r1e - r1s) * (r2e - r2s) + r2s;
}

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
  vec2 uv = vec2(gl_GlobalInvocationID.xy) / (gl_NumWorkGroups.xy * gl_WorkGroupSize.xy);
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  if (any(greaterThanEqual(texel, imageSize(u_outTex)))) return;
  vec3 N = uv2normal(uv);

  vec3 irradiance = vec3(0.0);

  vec3 up = vec3(0.0, 1.0, 0.0);
  vec3 right = cross(up, N);
  up = cross(N, right);

  float sampleDelta = 0.025;
  float nrSamples = 0.0;
  for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
  {
    for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
    {
      vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
      vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

      vec2 uv = normal2uv(sampleVec);
      //uv = smoothstep(vec2(0.25), vec2(1.0), uv);
      //uv.x = map(uv.x, 0, 1, .3, .7);
      //uv.y = map(uv.y, 0, 1, .1, .9);
      irradiance += texture(u_environment, uv).rgb * cos(theta) * sin(theta);
      nrSamples++;
    }
  }

  irradiance = PI * irradiance * (1.0 / float(nrSamples));
  imageStore(u_outTex, texel, vec4(irradiance, 1.0));
}