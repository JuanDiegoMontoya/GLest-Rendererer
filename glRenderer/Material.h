#pragma once
#include <unordered_map>
#include <optional>
#include <string>

class Texture2D;

struct Material
{
  Texture2D* diffuseTex{};
  Texture2D* specularTex{};
  Texture2D* normalTex{};
  float shininess{};
};

// sent to GPU
struct BindlessMaterial
{
  uint64_t diffuseHandle{};
  uint64_t specularHandle{};
  uint64_t normalHandle{};
  float shininess{};
  float _pad00{};
};

class MaterialManager
{
public:
  MaterialManager() {}
  ~MaterialManager();
  std::optional<Material> GetMaterial(const std::string& mat);
  Material& MakeMaterial(std::string name,
    std::string diffuseTexName,
    std::string specularTexName,
    std::string normalTexName,
    float shininess);
  std::vector<std::pair<std::string, Material>> GetLinearMaterials()
  {
    return { materials.begin(), materials.end() };
  }

private:
  std::unordered_map<std::string, Material> materials;
};