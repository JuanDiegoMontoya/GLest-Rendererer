#pragma once
#include <glm/glm.hpp>

class Frustum;

class Camera
{
public:
  Camera();

  void UpdateViewMat();
  void GenProjection();
  void Update(float dt);

  const glm::mat4& GetView() { return view_; }
  const glm::mat4& GetProj() { return proj_; }
  const glm::mat4& GetViewProj() { return viewProj_; }
  const glm::vec3& GetPos() { return worldpos_; }
  const glm::vec3& GetDir() { return dir_; }
  //const Frustum* GetFrustum() { return frustum_.get(); }
  float GetFov() { return fovDeg_; }
  float GetNear() { return near_; }
  float GetFar() { return far_; }
  glm::vec3 GetEuler() { return { pitch_, yaw_, roll_ }; }
  auto GetFront() { return front; }
  auto GetUp() { return up; }
  auto GetRight() { return right; }

  void SetPos(const glm::vec3& v) { worldpos_ = v; dirty_ = true; }
  void SetFar(float f) { far_ = f; GenProjection(); }
  void SetFront(const glm::vec3& f) { front = f; dirty_ = true; }
  void SetDir(const glm::vec3& v) { dir_ = v; dirty_ = true; }
  void SetYaw(float f) { yaw_ = f; dirty_ = true; }
  void SetPitch(float f) { pitch_ = f; dirty_ = true; }

private:
  glm::vec3 worldpos_ = glm::vec3(0, 0, 0);
  glm::vec3 dir_ = glm::vec3(-.22f, .22f, -.95f);

  glm::vec3 up = glm::vec3(0, 1, 0);
  glm::vec3 front = glm::vec3(0, 0, -1);
  glm::vec3 right = glm::vec3(1, 0, 0);

  float speed_ = 3.5f;

  // view matrix info
  float pitch_ = 16;
  float yaw_ = 255;
  float roll_ = 0;

  // projection matrix info
  float fovDeg_ = 90.f;
  float near_ = .1f;
  float far_ = 300.f;

  glm::mat4 view_ = glm::mat4(1);
  glm::mat4 proj_{};
  glm::mat4 viewProj_{}; // cached
  //std::unique_ptr<Frustum> frustum_;

  bool dirty_ = true;
};
