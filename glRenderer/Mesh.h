#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Texture2D;

struct Vertex
{
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec2 uv{};
  glm::vec3 tangent{};
  //glm::vec3 bitangent; // computed in vertex shader

  bool operator==(const Vertex& v) const&
  {
    return position == v.position && normal == v.normal &&
      uv == v.uv;
  }
  //bool operator<=>(const Vertex&) const& = default;
};

struct Material
{
  Texture2D* diffuseTex{};
  Texture2D* specularTex{};
  Texture2D* normalTex{};
  bool hasSpecular{};
  bool hasNormal{};
  float shininess{};
};

class Mesh
{
public:
  Mesh() {}
  Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Material& mat);
  ~Mesh();
  Mesh(Mesh&& other) noexcept : material(other.material)
  {
    this->vboID = std::exchange(other.vboID, 0);
    this->eboID = std::exchange(other.eboID, 0);
    this->vertexCount = std::exchange(other.vertexCount, 0);
  }
  Mesh& operator=(Mesh&& other) noexcept
  {
    if (&other == this) return *this;
    new(this) Mesh(std::move(other));
  }
  unsigned GetVBOID() const { return vboID; }
  unsigned GetEBOID() const { return eboID; }
  unsigned GetVertexCount() const { return vertexCount; }
  const Material& GetMaterial() const { return material; }

private:
  //std::vector<Vertex> vertices;
  unsigned vboID{};
  unsigned eboID{};
  unsigned vertexCount{};
  Material material{};
};

/// <summary>
/// Batch/bindless rendering
/// </summary>
struct MeshInfo
{

};

class BatchMesh
{
public:

private:
  MeshInfo* meshInfo_{};
  Material* material_{};
};

std::vector<Mesh> LoadObj(std::string path);