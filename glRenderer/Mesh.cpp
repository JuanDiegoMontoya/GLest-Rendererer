#include "Mesh.h"
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include <glad/glad.h>
#include "Texture.h"
#include <map>

Mesh::Mesh(const std::vector<Vertex>& vertices, Material mat) : vertexCount(vertices.size()), material(mat)
{
  glCreateBuffers(1, &id);
  glNamedBufferStorage(id, vertexCount * sizeof(Vertex), vertices.data(), 0);
}

Mesh::~Mesh()
{
  glDeleteBuffers(1, &id);
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

  if (!reader.Warning().empty())
  {
    std::cout << "TinyObjReader: " << reader.Warning();
  }

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
        
        vertices.push_back({ {vx, vy, vz}, { nx, ny, nz }, {tx, ty} });
      }
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
      vertices[vertices.size() - 3].tangent = tangent;
      vertices[vertices.size() - 2].tangent = tangent;
      vertices[vertices.size() - 1].tangent = tangent;
      vertices[vertices.size() - 3].bitangent = bitangent;
      vertices[vertices.size() - 2].bitangent = bitangent;
      vertices[vertices.size() - 1].bitangent = bitangent;
      index_offset += fv;

      // per-face material
      shapes[s].mesh.material_ids[f];

      // assume all faces in shape have same material
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

      if (prevName != name || f == shapes[s].mesh.num_face_vertices.size() - 1)
      {
        if (f == shapes[s].mesh.num_face_vertices.size() - 1)
        {
          prevName = name;
        }
        Material material{};
        if (auto findResult = materialMap.find(prevName); findResult != materialMap.end())
        {
          material = findResult->second;
        }
        else
        {
          std::cout << "Creating material: " << prevName << std::endl;
          material.diffuseTex = new Texture2D(diffuseName);
          material.alphaMaskTex = new Texture2D(maskName);
          material.specularTex = new Texture2D(specularName);
          material.normalTex = new Texture2D(normalName);
          material.hasSpecular = material.specularTex->Valid();
          material.hasAlpha = material.alphaMaskTex->Valid();
          material.hasNormal = material.normalTex->Valid();
          material.shininess = shininess;
          materialMap[prevName] = material;
        }
        meshes.emplace_back(vertices, material);
        vertices.clear();
      }
      prevName = name;
    }

  }

  return meshes;
}
