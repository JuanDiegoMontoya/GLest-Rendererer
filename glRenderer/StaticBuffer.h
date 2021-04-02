#pragma once
#include <cinttypes>

// RAII buffer wrapper
class StaticBuffer
{
public:
  StaticBuffer(const void* data, size_t size, uint32_t glflags);
  StaticBuffer(StaticBuffer&& other) noexcept;
  ~StaticBuffer();

  StaticBuffer(const StaticBuffer& other) = delete;
  StaticBuffer& operator=(const StaticBuffer&) = delete;
  StaticBuffer& operator=(StaticBuffer&&) = delete;
  bool operator==(const StaticBuffer&) const = default;

  void SubData(const void* data, size_t size, size_t offset);

  void Bind(uint32_t target);
  void BindBase(uint32_t target, uint32_t index);

  uint32_t ID() { return id_; }

private:
  uint32_t id_ = 0;
};