#pragma once

namespace pubsub
{
namespace pipeline
{

/// Base class which just has a virtual destructor
struct HolderBase
{
  virtual ~HolderBase() {};
  
  // todo work to remove the need for this
  virtual void* get() = 0;
  
  virtual HolderBase* clone() = 0;
};

}
}
