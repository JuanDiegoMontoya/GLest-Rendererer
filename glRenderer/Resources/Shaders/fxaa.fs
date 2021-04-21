#version 460 core

layout (location = 0) in vec2 vTexCoord;

layout (location = 0, binding = 0) uniform sampler2D colorTex;
layout (location = 1) uniform vec2 u_invScreenSize;
layout (location = 2) uniform float u_contrastThreshold = .0312; // .0833, .0625, .0312 from lowest to best quality
layout (location = 3) uniform float u_relativeThreshold = .125; // .250, .166, .125
layout (location = 4) uniform float u_blendStrength = 1.0;

layout (location = 0) out vec4 fragColor;

float ColorToLum(vec3 c)
{
  return dot(c, vec3(.30, .59, .11));
}

vec3 ComputeFXAA()
{
  // get immediate neighborhood
  vec3 colorCenter = textureOffset(colorTex, vTexCoord, ivec2(0, 0)).rgb;
  vec3 colorN = textureOffset(colorTex, vTexCoord, ivec2(0, 1)).rgb;
  vec3 colorS = textureOffset(colorTex, vTexCoord, ivec2(0, -1)).rgb;
  vec3 colorW = textureOffset(colorTex, vTexCoord, ivec2(1, 0)).rgb;
  vec3 colorE = textureOffset(colorTex, vTexCoord, ivec2(-1, 0)).rgb;
  float lumCenter = ColorToLum(colorCenter);
  float lumN = ColorToLum(colorN);
  float lumS = ColorToLum(colorS);
  float lumW = ColorToLum(colorW);
  float lumE = ColorToLum(colorE);

  float lumMax = max(max(max(max(lumCenter, lumN), lumS), lumW), lumS);
  float lumMin = min(min(min(min(lumCenter, lumN), lumS), lumW), lumS);
  float lumContrast = lumMax - lumMin;

  float threshold = max(u_contrastThreshold, u_relativeThreshold * lumMax);
  if (lumContrast < threshold)
  {
    return colorCenter;
  }

  // get diagonal neighborhood
  vec3 colorNW = textureOffset(colorTex, vTexCoord, ivec2(-1, 1)).rgb;
  vec3 colorNE = textureOffset(colorTex, vTexCoord, ivec2(1, 1)).rgb;
  vec3 colorSW = textureOffset(colorTex, vTexCoord, ivec2(-1, -1)).rgb;
  vec3 colorSE = textureOffset(colorTex, vTexCoord, ivec2(-1, 1)).rgb;
  float lumNW = ColorToLum(colorNW);
  float lumNE = ColorToLum(colorNE);
  float lumSW = ColorToLum(colorSW);
  float lumSE = ColorToLum(colorSE);

  // double weight given to immediate neighborhood, single weight given to corners
  float blendFactor = 2 * (lumN + lumS + lumW + lumE) + (lumNW + lumNE + lumSW + lumSE);
  blendFactor /= 12.0; // divide by sum of weights
  blendFactor = abs(blendFactor - lumCenter);
  blendFactor = clamp(blendFactor / lumContrast, 0.0, 1.0);
  blendFactor = smoothstep(0.0, 1.0, blendFactor);
  blendFactor *= u_blendStrength;

  // determine direction to blend
  float horizontalGrad = 
    abs(lumN + lumS - 2.0 * lumCenter) * 2.0 +
    abs(lumNE + lumSE - 2.0 * lumE) +
    abs(lumNW + lumSW - 2.0 * lumW);
  float verticalGrad = 
    abs(lumE + lumW - 2.0 * lumCenter) * 2.0 +
    abs(lumNE + lumNW - 2.0 * lumN) + 
    abs(lumSE + lumSW - 2.0 * lumS);
  bool isEdgeHorizontal = horizontalGrad >= verticalGrad;

  float posLum = isEdgeHorizontal ? lumN : lumE;
  float negLum = isEdgeHorizontal ? lumS : lumW;
  float posGrad = abs(posLum - lumCenter);
  float negGrad = abs(negLum - lumCenter);

  float texelStep = posGrad >= negGrad ? 1.0 : -1.0;

  vec2 offset;
  if (isEdgeHorizontal)
  {
    offset = vec2(0.0, texelStep * blendFactor * u_invScreenSize.y);
  }
  else
  {
    offset = vec2(texelStep * blendFactor * u_invScreenSize.x, 0.0);
  }

  return textureLod(colorTex, vTexCoord + offset, 0).rgb;
}

void main()
{
  fragColor = vec4(ComputeFXAA(), 1.0);
}