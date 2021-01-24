#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

class Mesh
{
public:
  Mesh(std::string objPath);
  ~Mesh();
  Mesh(Mesh&& other) noexcept
  {
    this->id = std::exchange(other.id, 0);
    this->vertexCount = std::exchange(other.vertexCount, 0);
  }
  unsigned GetID() const { return id; }
  unsigned GetVertexCount() const { return vertexCount; }

private:
  //std::vector<Vertex> vertices;
  unsigned id{};
  unsigned vertexCount{};
};

