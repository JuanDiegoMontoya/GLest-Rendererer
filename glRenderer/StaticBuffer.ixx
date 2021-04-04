module;

#include <cinttypes>
#include <utility>
#include "glad/glad.h"

export module GPU.StaticBuffer;

// RAII buffer wrapper
export class StaticBuffer
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


StaticBuffer::StaticBuffer(const void* data, size_t size, uint32_t glflags)
{
  glCreateBuffers(1, &id_);
  glNamedBufferStorage(id_, static_cast<GLsizeiptr>(size), data, static_cast<GLbitfield>(glflags));
}

StaticBuffer::StaticBuffer(StaticBuffer&& other) noexcept
{
  id_ = std::exchange(other.id_, 0);
}

StaticBuffer::~StaticBuffer()
{
  glDeleteBuffers(1, &id_);
}

void StaticBuffer::Bind(uint32_t target)
{
  glBindBuffer(target, id_);
}

void StaticBuffer::BindBase(uint32_t target, uint32_t index)
{
  glBindBuffer(target, id_);
  glBindBufferBase(target, index, id_);
}

void StaticBuffer::SubData(const void* data, size_t size, size_t offset)
{
  glNamedBufferSubData(id_, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
}