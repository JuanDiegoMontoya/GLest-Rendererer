#include "Mesh.h"
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include <glad/glad.h>
#include "Texture.h"
#include <map>
#include <unordered_map>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

inline void hash_combine(std::size_t& seed) {}

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hash_combine(seed, rest...);
}

namespace std
{
  template<> struct hash<Vertex>
  {
    std::size_t operator()(const Vertex& v) const noexcept
    {
      std::size_t h1 = std::hash<glm::vec3>{}((v.position));
      std::size_t h2 = std::hash<glm::vec3>{}((v.normal));
      std::size_t h3 = std::hash<glm::vec2>{}((v.uv));
      std::size_t seed = 0;
      hash_combine(seed, h1, h2, h3);
      return seed;
    }
  };
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Material& mat)
  : vertexCount(indices.size()), material(mat)
{
  glCreateBuffers(1, &vboID);
  glCreateBuffers(1, &eboID);
  glNamedBufferStorage(vboID, vertices.size() * sizeof(Vertex), vertices.data(), 0);
  glNamedBufferStorage(eboID, indices.size() * sizeof(uint32_t), indices.data(), 0);
}

Mesh::~Mesh()
{
  glDeleteBuffers(1, &vboID);
  glDeleteBuffers(1, &eboID);
}

std::vector<Mesh> LoadObj(std::string path)
{
  std::vector<Mesh> meshes;
  std::map<std::string, Material> materialMap;

  std::string texPath;
  if (size_t pos = path.find_last_of("/\\"); pos != std::string::npos)
  {
    texPath = path.substr(0, pos + 1);
  }

  tinyobj::ObjReaderConfig reader_config;
  //reader_config.mtl_search_path = "./Resources/Models"; // Path to material files
  reader_config.triangulate = true;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(path, reader_config))
  {
    if (!reader.Error().empty())
    {
      std::cerr << "TinyObjReader: " << reader.Error();
    }
    exit(1);
  }

  //if (!reader.Warning().empty())
  //{
  //  std::cout << "TinyObjReader: " << reader.Warning();
  //}

  auto& attrib = reader.GetAttrib();
  auto& shapes = reader.GetShapes();
  auto& materials = reader.GetMaterials();


  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++)
  {
    std::vector<Vertex> vertices;

    // Loop over faces(polygon)
    std::string prevName = materials.empty() ? "" : materials[shapes[s].mesh.material_ids[0]].name;
    size_t index_offset = 0;
    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
    {
      int fv = shapes[s].mesh.num_face_vertices[f];

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++)
      {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        tinyobj::real_t nx{};
        tinyobj::real_t ny{};
        tinyobj::real_t nz{};
        if (idx.normal_index >= 0)
        {
          nx = attrib.normals[3 * idx.normal_index + 0];
          ny = attrib.normals[3 * idx.normal_index + 1];
          nz = attrib.normals[3 * idx.normal_index + 2];
        }
        tinyobj::real_t tx{};
        tinyobj::real_t ty{};
        if (idx.texcoord_index >= 0)
        {
          tx = attrib.texcoords[2 * idx.texcoord_index + 0];
          ty = attrib.texcoords[2 * idx.texcoord_index + 1];
        }

        Vertex vertex{ {vx, vy, vz}, { nx, ny, nz }, {tx, ty} };
        vertices.push_back(vertex);
      }
      index_offset += fv;

      // calculate triangle tangents and bitangents
      // makes the BIG assumption that all faces have exactly three (3) vertices
      glm::vec3 pos1 = vertices[vertices.size() - 3].position;
      glm::vec3 pos2 = vertices[vertices.size() - 2].position;
      glm::vec3 pos3 = vertices[vertices.size() - 1].position;
      glm::vec2 uv1 = vertices[vertices.size() - 3].uv;
      glm::vec2 uv2 = vertices[vertices.size() - 2].uv;
      glm::vec2 uv3 = vertices[vertices.size() - 1].uv;
      glm::vec3 edge1 = pos2 - pos1;
      glm::vec3 edge2 = pos3 - pos1;
      glm::vec2 deltaUV1 = uv2 - uv1;
      glm::vec2 deltaUV2 = uv3 - uv1;
      float ff = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
      glm::vec3 tangent, bitangent;
      tangent.x = ff * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
      tangent.y = ff * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
      tangent.z = ff * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
      bitangent.x = ff * (-deltaUV2.y * edge1.x + deltaUV1.y * edge2.x);
      bitangent.y = ff * (-deltaUV2.y * edge1.y + deltaUV1.y * edge2.y);
      bitangent.z = ff * (-deltaUV2.y * edge1.z + deltaUV1.y * edge2.z);

      // correct tangents for each vertex normal
      //glm::vec3 e1 = glm::normalize(edge1);
      //glm::vec3 e2 = glm::normalize(edge2);
      //glm::vec3 faceNorm = glm::cross(e1, e2);
      //for (int i = 0; i < 3; i++)
      //{
      //  const glm::vec3 vertexNorm = vertices[vertices.size() - (3 - i)].normal;
      //  const float angle = glm::acos(glm::dot(faceNorm, vertexNorm));
      //  const glm::vec3 axis = -glm::cross(faceNorm, vertexNorm);
      //  const glm::mat4 rot = glm::rotate(glm::mat4(1), angle, axis);
      //  const glm::vec3 vertexTangent = rot * glm::vec4(tangent, 0.0f);
      //  vertices[vertices.size() - (3 - i)].tangent = tangent; // todo
      //  vertices[vertices.size() - (3 - i)].normal = vertexNorm;
      //}

      //vertices[vertices.size() - 3].tangent = tangent;
      //vertices[vertices.size() - 2].tangent = tangent;
      //vertices[vertices.size() - 1].tangent = tangent;
      //vertices[vertices.size() - 3].bitangent = bitangent;
      //vertices[vertices.size() - 2].bitangent = bitangent;
      //vertices[vertices.size() - 1].bitangent = bitangent;

      std::string name;
      std::string diffuseName;
      std::string maskName;
      std::string specularName;
      std::string normalName;
      float shininess = 1.0f;
      if (!materials.empty())
      {
        name = materials[shapes[s].mesh.material_ids[f]].name;
        diffuseName = texPath + materials[shapes[s].mesh.material_ids[f]].diffuse_texname;
        maskName = texPath + materials[shapes[s].mesh.material_ids[f]].alpha_texname;
        specularName = texPath + materials[shapes[s].mesh.material_ids[f]].specular_texname;
        normalName = texPath + materials[shapes[s].mesh.material_ids[f]].normal_texname;
        shininess = materials[shapes[s].mesh.material_ids[f]].shininess;
      }

      // push material and cut shape if material changes partway through
      if (prevName != name || f == shapes[s].mesh.num_face_vertices.size() - 1)
      {
        if (f == shapes[s].mesh.num_face_vertices.size() - 1)
        {
          prevName = name;
        }
        else
        {
          //printf("Shape split!\n");
        }
        Material material{};
        if (auto findResult = materialMap.find(prevName); findResult != materialMap.end())
        {
          material = findResult->second;
        }
        else
        {
          std::cout << "Creating material: " << prevName << std::endl;
          material.diffuseTex = new Texture2D(diffuseName, true, true);
          material.specularTex = new Texture2D(specularName, true, true);
          material.normalTex = new Texture2D(normalName, false, true);
          material.hasSpecular = material.specularTex->Valid();
          material.hasNormal = material.normalTex->Valid();
          material.shininess = shininess;
          materialMap[prevName] = material;
        }

        uint32_t currentVertexIndex = 0;
        std::unordered_map<Vertex, uint32_t> verticesUnique;
        std::vector<uint32_t> indices;
        for (const auto& vertex : vertices)
        {
          auto pp = verticesUnique.insert({ vertex, currentVertexIndex });
          if (pp.second) // insertion took place
          {
            currentVertexIndex++;
          }
          indices.push_back(pp.first->second); // index mapped to that vertex
        }

        std::vector<Vertex> vertices2;
        std::vector<std::pair<Vertex, uint32_t>> orderedVertices(verticesUnique.begin(), verticesUnique.end());
        std::sort(orderedVertices.begin(), orderedVertices.end(),
          [](const auto& p1, const auto& p2) { assert(p1.second != p2.second); return p1.second < p2.second; });
        for (const auto& p : orderedVertices)
        {
          vertices2.push_back(p.first);
        }
        meshes.emplace_back(vertices2, indices, material);

        // test
        //{
        //  indices.clear();
        //  for (uint32_t i = 0; i < vertices.size(); i++)
        //    indices.push_back(i);
        //  meshes.emplace_back(vertices, indices, material);
        //}
        vertices.clear();
      }
      prevName = name;
    }

  }

  return meshes;
}
