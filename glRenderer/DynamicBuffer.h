#pragma once
#include <stdint.h>
#include <vector>
#include "Utilities.h"
#include <glad/glad.h>
#include <algorithm>

// Generic GPU buffer that can store
// up to 4GB (UINT32_MAX) of data
class DynamicBuffer
{
public:
  DynamicBuffer(uint32_t size, uint32_t alignment);
  ~DynamicBuffer();

  // allocates a chunk of memory in the data store, returns handle to memory
  // the handle is used to free the chunk when the user is done with it
  uint64_t Allocate(const void* data, size_t size);

  // frees a chunk of memory being "pointed" to by a handle
  // returns true if the memory was able to be freed, false otherwise
  bool Free(uint64_t handle);

  // frees the oldest allocated chunk
  // returns handle to freed chunk, 0 if nothing was freed
  uint64_t FreeOldest();

  // query information about the allocator
  const auto& GetAlloc(uint64_t handle) { return *std::find_if(allocs_.begin(), allocs_.end(), [=](const auto& alloc) { return alloc.handle == handle; }); }
  const auto& GetAllocs() { return allocs_; }
  GLuint ActiveAllocs() { return numActiveAllocs_; }
  GLuint GetBufferHandle() { return buffer; }

  // compare return values of this func to see if the state has change
  std::pair<uint64_t, GLuint> GetStateInfo() { return { nextHandle, numActiveAllocs_ }; }

  const GLsizei align_; // allocation alignment

  struct allocationData
  {
    //allocationData() = default;
    //allocationData() {}

    uint64_t handle{}; // "pointer"
    double time{};     // time of allocation
    uint32_t flags{};  // GPU flags
    uint32_t _pad{};   // GPU padding
    uint32_t offset{}; // offset from beginning of this memory
    uint32_t size{};   // allocation size
  };

  constexpr size_t AllocSize() const { return sizeof(allocationData); }

protected:
  std::vector<allocationData> allocs_;
  using Iterator = decltype(allocs_.begin());

  // called whenever anything about the allocator changed
  void stateChanged();

  // merges null allocations adjacent to iterator
  void maybeMerge(Iterator it);

  // verifies the buffer has no errors, debug only
  void dbgVerify();

  GLuint buffer;
  uint64_t nextHandle = 1;
  GLuint numActiveAllocs_ = 0;
  const GLuint capacity_; // for fixed size buffers
  Timer timer;
};

#include "DynamicBuffer.inl"