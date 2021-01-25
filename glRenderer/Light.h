#pragma once
#include <glm/glm.hpp>

#define LIGHT_POINT 0
#define LIGHT_SPOT 1

struct PointLight
{
  glm::vec4 diffuse;
  glm::vec4 specular;

  // point + spot light
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
    float root2 = (-L - discriminant) / (2 * Q);
    return luminance * glm::pow(glm::max(root1, root2), 2.0f);
  }
};

struct DirLight
{
  glm::vec3 ambient;
  glm::vec3 diffuse;
  glm::vec3 specular;

  // directional + spot light
  glm::vec3 direction;
};