#include "Material.h"
#include "Texture.h"

MaterialManager::~MaterialManager()
{
  for (auto& p : materials)
  {
    delete p.second.diffuseTex;
    delete p.second.specularTex;
    delete p.second.normalTex;
  }
}

std::optional<Material> MaterialManager::GetMaterial(const std::string& mat)
{
  auto it = materials.find(mat);
  if (it == materials.end())
  {
    return std::optional<Material>();
  }
  return it->second;
}

Material& MaterialManager::MakeMaterial(std::string name, std::string diffuseTexName, std::string specularTexName, std::string normalTexName, float shininess)
{
  if (auto it  = materials.find(name); it != materials.end())
  {
    return it->second;
  }

  Material material;
  material.diffuseTex = new Texture2D(diffuseTexName, true, true);
  material.specularTex = new Texture2D(specularTexName, true, true);
  material.normalTex = new Texture2D(normalTexName, false, true);
  material.shininess = shininess;
  auto p = materials.insert({ name, material });
  return p.first->second;
}
