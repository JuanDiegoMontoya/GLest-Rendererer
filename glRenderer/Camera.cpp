#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "Input.h"

Camera::Camera()
{
	UpdateViewMat();
}

void Camera::UpdateViewMat()
{
  view_ = glm::lookAt(worldpos_, worldpos_ + front, { 0,1,0 });
  viewProj_ = proj_ * view_;
  //frustum_->Transform(proj_, view_);
  dirty_ = false;
}

void Camera::GenProjection()
{
  proj_ = glm::perspective(glm::radians(fovDeg_), 1920.f / 1080.f, near_, far_);
  dirty_ = true;
}

void Camera::Update(float dt)
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
