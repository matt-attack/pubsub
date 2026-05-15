#pragma once

namespace pubsub
{
struct DefaultAllocator
{
  static ps_allocator_t* allocator()
  {
    return &ps_default_allocator;
  }
};

}
