
#pragma once

#include <vector>

// Vector like array that's able to take ownership of C malloced arrays
template <class T>
class ArrayVector
{
	T* data_;
	uint32_t length_;
	uint32_t capacity_;
public:

	ArrayVector()
	{
		length_ = capacity_ = 0;
		data_ = 0;
	}

	ArrayVector(T* arr, uint32_t len)
	{
		data_ = arr;
		length_ = len;
		capacity_ = len;
	}

	ArrayVector(const ArrayVector<T>& obj)
	{
		length_ = obj.length_;
		capacity_ = obj.capacity_;
		data_ = (T*)malloc(sizeof(T)*length_);
		for (int i = 0; i < length_; i++)
		{
			data_[i] = obj[i];
		}
	}

	~ArrayVector()
	{
		if (data_)
		{
			free(data_);
		}
	}

	ArrayVector<T>& operator=(const ArrayVector<T>& obj)
	{
		if (data_)
			free(data_);

		length_ = obj.length_;
		capacity_ = obj.capacity_;
		data_ = (T*)malloc(sizeof(T)*length_);
		for (int i = 0; i < length_; i++)
		{
			data_[i] = obj[i];
		}
		return *this;
	}

	ArrayVector<T>& operator=(const std::vector<T>& arr)
	{
		if (data_)
			free(data_);

		capacity_ = length_ = arr.size();
		data_ = (T*)malloc(sizeof(T)*length_);
		for (int i = 0; i < length_; i++)
		{
			data_[i] = arr[i];
		}
		return *this;
	}

	void reserve(const uint32_t l)
	{
		if (l <= capacity_) { return; }
		auto n = (T*)malloc(sizeof(T)*l);
		capacity_ = l;
		for (int i = 0; i < length_; i++)
		{
			n[i] = std::move(data_[i]);
		}
		data_ = n;
	}

	void push_back(const T& v)
	{
		if (length_ + 1 > capacity_)
		{
			reserve(capacity_*2);// good enough for now
		}

		data_[length_++] = v;
	}

	inline T& operator[](const uint32_t index) { return data_[length_]; }

	inline T* data() const { return data_; }

	inline uint32_t size() const { return length_; }
};
