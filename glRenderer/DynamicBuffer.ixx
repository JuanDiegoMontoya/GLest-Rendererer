module;

#include <stdint.h>
#include <vector>
#include <algorithm>
#include <glad/glad.h>

export module GPU.DynamicBuffer;

import Utilities;

// Generic GPU buffer that can store
// up to 4GB (UINT32_MAX) of data
export class DynamicBuffer
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


DynamicBuffer::DynamicBuffer(uint32_t size, uint32_t alignment)
  : align_(alignment), capacity_(size)
{
  // align
  size += (align_ - (size % align_)) % align_;

  // allocate uninitialized memory in VRAM
  //buffer = std::make_unique<StaticBuffer>(nullptr, size);
  glCreateBuffers(1, &buffer);
  glNamedBufferStorage(buffer, std::max(uint32_t(1), size), nullptr, GL_DYNAMIC_STORAGE_BIT);

  // make one big null allocation
  DynamicBuffer::allocationData falloc
  {
    .handle = NULL,
    .time = 0,
    .offset = 0,
    .size = size,
  };
  allocs_.push_back(falloc);
}

DynamicBuffer::~DynamicBuffer()
{
  glDeleteBuffers(1, &buffer);
}

uint64_t DynamicBuffer::Allocate(const void* data, size_t size)
{
  size += (align_ - (size % align_)) % align_;
  // find smallest NULL allocation that will fit
  Iterator small = allocs_.end();
  for (int i = 0; i < allocs_.size(); i++)
  {
    if (allocs_[i].handle == NULL && allocs_[i].size >= size) // potential allocation
    {
      if (small == allocs_.end()) // initialize small
      {
        small = allocs_.begin() + i;
      }
      else if (allocs_[i].size < small->size)
      {
        small = allocs_.begin() + i;
      }
    }
  }
  // allocation failure
  if (small == allocs_.end())
  {
    return NULL;
  }

  // split free allocation
  allocationData newAlloc
  {
    .handle = nextHandle++,
    .time = timer.elapsed(),
    .flags = 0,
    .offset = small->offset,
    .size = static_cast<uint32_t>(size),
  };

  small->offset += newAlloc.size;
  small->size -= newAlloc.size;

  // replace shrunk alloc if it would become degenerate
  if (small->size == 0)
  {
    *small = newAlloc;
  }
  else
  {
    allocs_.insert(small, newAlloc);
  }

  //buffer->SubData(data, newAlloc.size, newAlloc.offset);
  glNamedBufferSubData(buffer, newAlloc.offset, newAlloc.size, data);
  ++numActiveAllocs_;
  stateChanged();
  return newAlloc.handle;
}

bool DynamicBuffer::Free(uint64_t handle)
{
  if (handle == NULL) return false;
  auto it = std::find_if(allocs_.begin(), allocs_.end(), [&](const auto& a) { return a.handle == handle; });
  if (it == allocs_.end()) // failed to free
    return false;

  it->handle = NULL;
  maybeMerge(it);
  --numActiveAllocs_;
  stateChanged();
  return true;
}

uint64_t DynamicBuffer::FreeOldest()
{
  // find and free the oldest allocation
  Iterator old = allocs_.end();
  for (int i = 0; i < allocs_.size(); i++)
  {
    if (allocs_[i].handle != NULL)
    {
      if (old == allocs_.end())
        old = allocs_.begin() + i;
      else if (allocs_[i].time < old->time)
        old = allocs_.begin() + i;
    }
  }

  // failed to find old node to free
  if (old == allocs_.end())
    return NULL;

  auto retval = old->handle;
  old->handle = NULL;
  maybeMerge(old);
  --numActiveAllocs_;
  stateChanged();
  return retval;
}

void DynamicBuffer::stateChanged()
{
  //DEBUG_DO(dbgVerify());
}

void DynamicBuffer::maybeMerge(Iterator it)
{
  bool removeIt = false;
  bool removeNext = false;

  // merge with next alloc
  if (it != allocs_.end() - 1)
  {
    Iterator next = it + 1;
    if (next->handle == NULL)
    {
      it->size += next->size;
      removeNext = true;
    }
  }

  // merge with previous alloc
  if (it != allocs_.begin())
  {
    Iterator prev = it - 1;
    if (prev->handle == NULL)
    {
      prev->size += it->size;
      removeIt = true;
    }
  }

  // erase merged allocations
  if (removeIt && removeNext)
    allocs_.erase(it, it + 2); // this and next
  else if (removeIt)
    allocs_.erase(it);         // just this
  else if (removeNext)
    allocs_.erase(it + 1);     // just next
}

void DynamicBuffer::dbgVerify()
{
  //uint64_t prevPtr = 1;
  //GLsizei sumSize = 0;
  //GLuint active = 0;
  //for (const auto& alloc : allocs_)
  //{
  //  if (alloc.handle != NULL)
  //    active++;
  //  // check there are never two null blocks in a row
  //  ASSERT_MSG(!(prevPtr == NULL && alloc.handle == NULL),
  //    "Verify failed: two null blocks in a row!");
  //  prevPtr = alloc.handle;

  //  // check offset is equal to total size so far
  //  ASSERT_MSG(alloc.offset == sumSize,
  //    "Verify failed: size/offset discrepancy!");
  //  sumSize += alloc.size;

  //  // check alignment
  //  ASSERT_MSG(alloc.offset % align_ == 0,
  //    "Verify failed: block alignment mismatch!");

  //  // check degenerate (0-size) allocation
  //  ASSERT_MSG(alloc.size != 0,
  //    "Verify failed: 0-size allocation!");
  //}

  //ASSERT_MSG(active == numActiveAllocs_,
  //  "Verify failed: active allocations mismatch!");
}