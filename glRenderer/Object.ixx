module;

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

export module Object;

import Mesh;

export struct Transform
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

  glm::vec3 translation{ 0, 0, 0 };
  glm::quat rotation{ 1, 0, 0, 0 };
  glm::vec3 scale{ 1, 1, 1 };
};

export struct ObjectMeshed
{
  Transform transform;
  std::vector<Mesh*> meshes;
};

export struct ObjectBatched
{
  Transform transform;
  std::vector<MeshInfo> meshes;
};

#pragma warning(disable : 4324; suppress : 4324)
export struct alignas(16) ObjectUniforms // sent to GPU
{
  glm::mat4 modelMatrix{};
  //glm::mat4 normalMatrix{};
  uint32_t materialIndex{};
};