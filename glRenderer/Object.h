#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include "Mesh.h"

struct Transform
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
};

struct ObjectMeshed
{
  Transform transform;
  std::vector<Mesh*> meshes;
};

struct ObjectBatched
{
  Transform transform;
  std::vector<MeshInfo> meshes;
};

struct alignas(16) ObjectUniforms // sent to GPU
{
  glm::mat4 modelMatrix{};
  glm::mat4 normalMatrix{};
  uint32_t materialIndex{};
};