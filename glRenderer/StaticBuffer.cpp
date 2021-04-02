#include "StaticBuffer.h"
#include <utility>
#include "glad/glad.h"

StaticBuffer::StaticBuffer(const void* data, size_t size, uint32_t glflags)
{
  glCreateBuffers(1, &id_);
  glNamedBufferStorage(id_, size, data, glflags);
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
  glNamedBufferSubData(id_, offset, size, data);
}