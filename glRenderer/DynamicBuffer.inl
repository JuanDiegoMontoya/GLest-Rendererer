#pragma once

inline DynamicBuffer::DynamicBuffer(uint32_t size, uint32_t alignment)
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

inline DynamicBuffer::~DynamicBuffer()
{
  glDeleteBuffers(1, &buffer);
}

inline uint64_t DynamicBuffer::Allocate(const void* data, size_t size)
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


inline bool DynamicBuffer::Free(uint64_t handle)
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

  
inline uint64_t DynamicBuffer::FreeOldest()
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

  
inline inline void DynamicBuffer::stateChanged()
{
  //DEBUG_DO(dbgVerify());
}

  
inline void DynamicBuffer::maybeMerge(Iterator it)
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

  
inline void DynamicBuffer::dbgVerify()
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