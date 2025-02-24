
#pragma once

#include <stdexcept>
#include <string>
#include <string.h>

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

  operator const char*()
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

  bool operator==(const char* other)
  {
    return strcmp(other, data_) == 0;
  }

  bool operator==(const std::string& other)
  {
    return strcmp(other.c_str(), data_) == 0;
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

//would have to require the allocator in this class as part of the template
// Wrapper for a C string which makes it easier to use and handles freeing
// todo how to handle allocators?
class CString
{
  char* data_;
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
      data_ = strdup(other.data_);
    }
  }

  ~CString()
  {
    if (data_)
    {
      free(data_);
    }
  }
  
  void operator=(const std::string& string)
  {
    if (data_)
    {
      free(data_);
    }
    data_ = (char*)malloc(string.length()+1);
    strcpy(data_, string.c_str());
  }
  
  void operator=(const char* string)
  {
    if (data_)
    {
      free(data_);
    }
    data_ = (char*)malloc(strlen(string)+1);
    strcpy(data_, string);
  }

  void operator=(const CString& other)
  {
    if (data_)
    {
      free(data_);
    }
    if (other.data_)
    {
      data_ = strdup(other.data_);
    }
  }
  
  bool operator==(const char* other)
  {
    if (data_ == 0)
    {
      return strcmp(other, "") == 0;
    }
    return strcmp(other, data_) == 0;
  }

  bool operator==(const std::string& other)
  {
    if (data_ == 0)
    {
      return strcmp(other.c_str(), "") == 0;
    }
    return strcmp(other.c_str(), data_) == 0;
  }

  bool operator==(const CString& other)
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
