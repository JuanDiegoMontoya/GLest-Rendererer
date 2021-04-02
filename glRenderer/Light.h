#pragma once
#include <glm/glm.hpp>

// padded to send to GPU
struct PointLight
{
  glm::vec4 diffuse;
  glm::vec4 position;
  float linear;
  float quadratic;
  float radiusSquared;
  float _padding;

  float CalcRadiusSquared(float epsilon) const
  {
    // eps = 1 / (L * D + Q * D * D)
    // D = (-L +- sqrt(L^2 - 4(Q * -1/eps)))/(2Q)
    assert(epsilon > 0.0f);
    //float luminance = glm::max(glm::dot(glm::vec3(diffuse), glm::vec3(.3f, .59f, .11f)), glm::dot(glm::vec3(specular), glm::vec3(.3f, .59f, .11f)));
    float luminance = glm::max(diffuse.x, glm::max(diffuse.y, diffuse.z));
    float L = linear;
    float Q = quadratic;
    float E = epsilon;
    if (glm::epsilonEqual(quadratic, 0.f, .001f))
    {
      return luminance / (L * epsilon);
    }
    float discriminant = glm::sqrt(L * L - 4 * (Q * (-1 / E)));
    float root1 = (-L + discriminant) / (2 * Q);
    return luminance * glm::pow(root1, 2.0f);
  }
};

struct DirLight
{
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;
  glm::vec3 direction;
};

inline glm::mat4 MakeLightMatrix(const DirLight& light, glm::vec3 eye, glm::vec2 dim, glm::vec2 depthRange)
{
  glm::mat4 lightView = glm::lookAt(
    eye, eye + light.direction, glm::vec3(0, 1, 0));
  glm::mat4 lightProj = glm::ortho(
    -dim.x / 2, dim.x / 2, -dim.y / 2, dim.y / 2, 
    depthRange.x, depthRange.y);
  return lightProj * lightView;
}