#include "Mesh.h"
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>
#include <glad/glad.h>

Mesh::Mesh(std::string objPath)
{
  //std::vector<glm::vec3> positions;
  //std::vector<glm::vec2> uvs;
  //std::vector<glm::vec3> normals;
  //std::vector<tinyobj::index_t> indices;
  std::vector<Vertex> vertices; // final vertex


  tinyobj::ObjReaderConfig reader_config;
  reader_config.mtl_search_path = "./"; // Path to material files
  reader_config.triangulate = true;

  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(objPath, reader_config))
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
    // Loop over faces(polygon)
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
        // Optional: vertex colors
        // tinyobj::real_t red = attrib.colors[3*idx.vertex_index+0];
        // tinyobj::real_t green = attrib.colors[3*idx.vertex_index+1];
        // tinyobj::real_t blue = attrib.colors[3*idx.vertex_index+2];
        //positions.push_back({ vx, vy, vz });
        //uvs.push_back({ tx, ty });
        //normals.push_back({ nx, ny, nz });
        //indices.push_back(idx);
        vertices.push_back({ {vx, vy, vz}, {nx, ny, nz}, {tx, ty} });
      }
      index_offset += fv;

      // per-face material
      shapes[s].mesh.material_ids[f];
    }
  }

  //for (const auto& index : indices)
  //{
  //  Vertex v;
  //  v.position = positions[index.vertex_index];
  //  v.uv = uvs[index.texcoord_index];
  //  v.normal = normals[index.normal_index];
  //  vertices.push_back(v);
  //}

  glCreateBuffers(1, &id);
  glNamedBufferStorage(id, vertices.size() * sizeof(Vertex), vertices.data(), 0);
  vertexCount = vertices.size();
}

Mesh::~Mesh()
{
  glDeleteBuffers(1, &id);
}
