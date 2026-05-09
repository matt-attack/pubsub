
#pragma once

#include <pubsub_cpp/allocator.h>
#include <stdexcept>
#include <string>
#include <string.h>

namespace pubsub
{

#pragma pack(push, 1)
// Wrapper for a fixed size array for using it like a string
template <int string_length>
class FixedString
{
	char data_[string_length];
public:

  FixedString()
  {
    data_[0] = 0;
  }

  operator const char*() const
  {
    return data_;
  }
  
  void operator=(const std::string& string)
  {
    if (string.length() >= string_length)
    {
      throw std::runtime_error("Too big.");
    }
    strcpy(data_, string.c_str());
  }
  
  void operator=(const char* string)
  {
    // todo make this more efficient?
    if (strlen(string) >= string_length)
    {
      throw std::runtime_error("Too big.");
    }
    strncpy(data_, string, string_length - 1);
  }

  bool operator==(const char* other) const
  {
    return strcmp(other, data_) == 0;
  }

  bool operator==(const std::string& other) const
  {
    return strcmp(other.c_str(), data_) == 0;
  }
  
  bool operator==(const FixedString& other) const
  {
    return strcmp(other.c_str(), data_) == 0;
  }

  bool operator!=(const char* other) const
  {
    return strcmp(other, data_) != 0;
  }

  bool operator!=(const std::string& other) const
  {
    return strcmp(other.c_str(), data_) != 0;
  }

  bool operator!=(const FixedString& other) const
  {
    return strcmp(other.c_str(), data_) != 0;
  }
    
  char* data() const
  {
    return data_;
  }
  
  const char* c_str() const
  {
    return data_;
  }

  int max_size() const
  {
    return string_length;
  }
  
  int length() const
  {
    return strlen(data_);
  }
};

// Wrapper for a C string which makes it easier to use and handles freeing
template <class Allocator = DefaultAllocator>
class CString
{
  char* data_;

	// copy a buffer of a given length to a new allocated array
	static char* copy(const char* obj, uint32_t length)
	{
	  auto data = (char*)Allocator::allocator()->alloc(length, Allocator::allocator()->context);
		for (int i = 0; i < length; i++)
		{
			data[i] = obj[i];
		}
		return data;
	}
	
  static char* copy(const char* obj)
	{
	  auto length = strlen(obj) + 1;
	  auto data = (char*)Allocator::allocator()->alloc(length, Allocator::allocator()->context);
		for (int i = 0; i < length; i++)
		{
			data[i] = obj[i];
		}
		return data;
	}
public:

  CString()
  {
    data_ = 0;
  }
  
  CString(const CString& other)
  {
    data_ = 0;
    if (other.data_)
    {
      data_ = copy(other.data_);
    }
  }

  ~CString()
  {
    if (data_)
    {
      Allocator::allocator()->free(data_, Allocator::allocator()->context);
    }
  }
  
  void operator=(const std::string& string)
  {
    if (data_)
    {
      Allocator::allocator()->free(data_, Allocator::allocator()->context);
    }
    data_ = copy(string.c_str(), string.length()+1);
  }
  
  void operator=(const char* string)
  {
    if (data_)
    {
      Allocator::allocator()->free(data_, Allocator::allocator()->context);
    }
    data_ = copy(string);
  }

  void operator=(const CString& other)
  {
    if (data_)
    {
      Allocator::allocator()->free(data_, Allocator::allocator()->context);
      data_ = 0;
    }
    if (other.data_)
    {
      data_ = copy(other.data_);
    }
  }
  
  bool operator==(const char* other) const
  {
    if (data_ == 0)
    {
      return strcmp(other, "") == 0;
    }
    return strcmp(other, data_) == 0;
  }

  bool operator==(const std::string& other) const
  {
    if (data_ == 0)
    {
      return strcmp(other.c_str(), "") == 0;
    }
    return strcmp(other.c_str(), data_) == 0;
  }

  bool operator==(const CString& other) const
  {
    if (data_ == 0)
    {
      return strcmp(other.c_str(), "") == 0;
    }
    return strcmp(other.c_str(), data_) == 0;
  }
  
  char* data() const
  {
    return data_;
  }

  const char* c_str() const
  {
    if (data_ == 0)
    {
      return "";
    }
    return data_;
  }
};
#pragma pack(pop)
}
