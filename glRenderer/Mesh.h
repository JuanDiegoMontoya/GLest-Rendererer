#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "Texture.h"

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
  glm::vec3 tangent;
  glm::vec3 bitangent;
};

struct Material
{
  Texture2D* diffuseTex{};
  Texture2D* alphaMaskTex{};
  Texture2D* specularTex{};
  Texture2D* normalTex{};
  bool hasAlpha{};
  bool hasSpecular{};
  bool hasNormal{};
  float shininess{};
};

class Mesh
{
public:
  Mesh(const std::vector<Vertex>& vertices, Material mat);
  ~Mesh();
  Mesh(Mesh&& other) noexcept : material(other.material)
  {
    this->id = std::exchange(other.id, 0);
    this->vertexCount = std::exchange(other.vertexCount, 0);
  }
  unsigned GetID() const { return id; }
  unsigned GetVertexCount() const { return vertexCount; }
  const Material& GetMaterial() const { return material; }

private:
  //std::vector<Vertex> vertices;
  unsigned id{};
  unsigned vertexCount{};
  Material material;
};

std::vector<Mesh> LoadObj(std::string path);