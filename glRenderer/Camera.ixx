module;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Input.h"

export module Camera;

export class Camera
{
public:
  Camera()
  {
    UpdateViewMat();
  }

  void UpdateViewMat()
  {
    view_ = glm::lookAt(worldpos_, worldpos_ + front, { 0,1,0 });
    viewProj_ = proj_ * view_;
    //frustum_->Transform(proj_, view_);
    dirty_ = false;
  }

  void GenProjection()
  {
    proj_ = glm::perspective(glm::radians(fovDeg_), 1920.f / 1080.f, near_, far_);
    dirty_ = true;
  }

  void Update(float dt)
  {
    float speed = 3.5f;

    float currSpeed = speed * dt;
    if (Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT))
      currSpeed *= 10;
    if (Input::IsKeyDown(GLFW_KEY_LEFT_CONTROL))
      currSpeed /= 10;
    if (Input::IsKeyDown(GLFW_KEY_W))
      worldpos_ += currSpeed * GetFront();
    if (Input::IsKeyDown(GLFW_KEY_S))
      worldpos_ -= currSpeed * GetFront();
    if (Input::IsKeyDown(GLFW_KEY_A))
      worldpos_ -= glm::normalize(glm::cross(GetFront(), GetUp())) * currSpeed;
    if (Input::IsKeyDown(GLFW_KEY_D))
      worldpos_ += glm::normalize(glm::cross(GetFront(), GetUp())) * currSpeed;

    auto pyr = GetEuler();
    SetYaw(pyr.y + Input::GetScreenOffset().x);
    SetPitch(glm::clamp(pyr.x + Input::GetScreenOffset().y, -89.0f, 89.0f));

    glm::vec3 temp;
    temp.x = cos(glm::radians(pyr.x)) * cos(glm::radians(pyr.y));
    temp.y = sin(glm::radians(pyr.x));
    temp.z = cos(glm::radians(pyr.x)) * sin(glm::radians(pyr.y));
    SetFront(glm::normalize(temp));

    if (dirty_)
    {
      GenProjection();
      UpdateViewMat();
    }
  }


  const glm::mat4& GetView() const { return view_; }
  const glm::mat4& GetProj() const { return proj_; }
  const glm::mat4& GetViewProj() const { return viewProj_; }
  const glm::vec3& GetPos() const { return worldpos_; }
  const glm::vec3& GetDir() const { return dir_; }
  //const Frustum* GetFrustum() { return frustum_.get(); }
  float GetFov() const { return fovDeg_; }
  float GetNear() const { return near_; }
  float GetFar() const { return far_; }
  glm::vec3 GetEuler() const { return { pitch_, yaw_, roll_ }; }
  const glm::vec3& GetFront() const { return front; }
  const glm::vec3& GetUp() const { return up; }
  const glm::vec3& GetRight() const { return right; }

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
  float near_ = .3f;
  float far_ = 300.f;

  glm::mat4 view_ = glm::mat4(1);
  glm::mat4 proj_{};
  glm::mat4 viewProj_{}; // cached
  //std::unique_ptr<Frustum> frustum_;

  bool dirty_ = true;
};
