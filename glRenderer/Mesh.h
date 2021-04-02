#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include "Material.h"
#include "DynamicBuffer.h"

struct Vertex
{
  glm::vec3 position{};
  glm::vec3 normal{};
  glm::vec2 uv{};
  glm::vec3 tangent{};
  //glm::vec3 bitangent; // computed in vertex shader

  bool operator==(const Vertex& v) const&
  {
    return position == v.position && normal == v.normal && uv == v.uv;
  }
  //bool operator<=>(const Vertex&) const& = default;
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

// Batch/bindless rendering
struct MeshInfo
{
  uint64_t verticesAllocHandle{};
  uint64_t indicesAllocHandle{};
  std::string materialName{};
  uint32_t materialIndex{};
};

struct MeshDescriptor
{
  std::vector<std::vector<Vertex>> vertices;
  std::vector<std::vector<uint32_t>> indices;
  std::vector<std::string> materials;
};
MeshDescriptor LoadObjBase(const std::string& path,
  MaterialManager& materialManager);
std::vector<Mesh> LoadObjMesh(const std::string& path,
  MaterialManager& materialManager);
std::vector<MeshInfo> LoadObjBatch(const std::string& path,
  MaterialManager& materialManager,
  DynamicBuffer& vertexBuffer,
  DynamicBuffer& indexBuffer);