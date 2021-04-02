#pragma once
#include <unordered_map>
#include <optional>
#include <string>

class Texture2D;

struct Material
{
  Texture2D* albedoTex{};
  Texture2D* roughnessTex{};
  Texture2D* metalnessTex{};
  Texture2D* normalTex{};
  Texture2D* ambientOcclusionTex{};
};

// sent to GPU
struct BindlessMaterial
{
  uint64_t albedoHandle{};
  uint64_t roughnessHandle{};
  uint64_t metalnessHandle{};
  uint64_t normalHandle{};
  uint64_t ambientOcclusionHandle{};
};

class MaterialManager
{
public:
  MaterialManager() {}
  ~MaterialManager();
  std::optional<Material> GetMaterial(const std::string& mat);
  Material& MakeMaterial(std::string name,
    std::string albedoTexName,
    std::string roughnessTexName,
    std::string metalnessTexName,
    std::string normalTexName,
    std::string ambientOcclusionTexName);
  std::vector<std::pair<std::string, Material>> GetLinearMaterials()
  {
    return { materials.begin(), materials.end() };
  }

private:
  std::unordered_map<std::string, Material> materials;
};