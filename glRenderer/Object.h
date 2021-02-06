#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

class Mesh;

struct Object
{
  glm::mat4 GetModelMatrix() const
  {
    glm::mat4 transform(1);
    transform = glm::translate(transform, translation);
    transform *= mat4_cast(rotation);
    transform = glm::scale(transform, scale);
    return transform;
  }

  glm::mat4 GetNormalMatrix() const
  {
    return glm::transpose(glm::inverse(glm::mat3(GetModelMatrix())));
  }

  glm::vec3 translation{};
  glm::quat rotation{1, 0, 0, 0};
  glm::vec3 scale{};

  std::vector<Mesh*> meshes;
};

