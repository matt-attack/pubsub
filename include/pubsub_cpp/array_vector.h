
#pragma once

#include <vector>
#include <pubsub_cpp/allocator.h>

namespace pubsub
{

#pragma pack(push, 1)
// Vector like array that's able to take ownership of C malloced arrays
template <class T, class Allocator = DefaultAllocator>
class ArrayVector
{
	T* data_;
	uint32_t length_;
	
	// copy a buffer of a given length to a new allocated array
	static T* copy(const T* obj, uint32_t length)
	{
	  auto data = (T*)Allocator::allocator()->alloc(sizeof(T)*length, Allocator::allocator()->context);
		for (int i = 0; i < length; i++)
		{
			data[i] = obj[i];
		}
		return data;
	}
	
public:

	ArrayVector()
	{
		length_ = 0;
		data_ = 0;
	}

	ArrayVector(T* arr, uint32_t len)
	{
		data_ = arr;
		length_ = len;
	}

	ArrayVector(const ArrayVector<T>& obj)
	{
		length_ = obj.length_;
		data_ = copy(obj.data(), length_);
	}

	~ArrayVector()
	{
		if (data_)
		{
			Allocator::allocator()->free(data_, Allocator::allocator()->context);
		}
	}

	ArrayVector<T>& operator=(const ArrayVector<T>& arr)
	{
		if (data_)
		{
			Allocator::allocator()->free(data_, Allocator::allocator()->context);
		}

		length_ = arr.length_;
		data_ = copy(arr.data(), length_);
		return *this;
	}

	ArrayVector<T>& operator=(const std::vector<T>& arr)
	{
		if (data_)
		{
			Allocator::allocator()->free(data_, Allocator::allocator()->context);
		}

		length_ = arr.size();
    data_ = copy(arr.data(), length_);
		return *this;
	}

	// resizes to the given side, leaving memory uninitalized for new values
	void resize(const uint32_t size)
	{
		if (size == length_) { return; }

		auto new_data = (T*)Allocator::allocator()->alloc(sizeof(T)*size, Allocator::allocator()->context);
		auto copy_len = std::min(size, length_);
		for (uint32_t i = 0; i < copy_len; i++)
		{
			new_data[i] = data_[i];
		}
		length_ = size;
		if (data_)
		{
			Allocator::allocator()->free(data_, Allocator::allocator()->context);
		}
		data_ = new_data;
	}
	
	// reliquinquishes the held pointer without freeing
	T* reset()
	{
	  auto out = data_;
	  data_ = 0;
	  length_ = 0;
	  return out;
	}

	void clear()
	{
		length_ = 0;
		Allocator::allocator()->free(data_, Allocator::allocator()->context);
		data_ = 0;
	}

	inline const T& operator[](const uint32_t index) const { return data_[index]; }

	inline T& operator[](const uint32_t index) { return data_[index]; }

	inline T* data() const { return data_; }

	inline uint32_t size() const { return length_; }

	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin() { return &data_[0]; }
	inline const_iterator begin() const { return &data_[0]; }
	inline iterator end() { return &data_[length_]; }
	inline const_iterator end() const { return &data_[length_]; }
};
#pragma pack(pop)
}
