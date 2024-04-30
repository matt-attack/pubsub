
#pragma once

#include <vector>


#pragma pack(push, 1)
// Vector like array that's able to take ownership of C malloced arrays
template <class T>
class ArrayVector
{
	T* data_;
	uint32_t length_;
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
		{
			free(data_);
		}

		length_ = obj.length_;
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
		{
			free(data_);
		}

		length_ = arr.size();
		data_ = (T*)malloc(sizeof(T)*length_);
		for (int i = 0; i < length_; i++)
		{
			data_[i] = arr[i];
		}
		return *this;
	}

	// resizes to the given side, leaving memory uninitalized for new values
	void resize(const uint32_t size)
	{
		if (size == length_) { return; }

		auto new_data = (T*)malloc(sizeof(T)*size);
		auto copy_len = std::min(size, length_);
		for (int i = 0; i < copy_len; i++)
		{
			new_data[i] = data_[i];
		}
		length_ = size;
		if (data_)
		{
			free(data_);
		}
		data_ = new_data;
	}

	void clear()
	{
		length_ = 0;
		free(data_);
		data_ = 0;
	}

	inline const T& operator[](const uint32_t index) const { return data_[index]; }

	inline T& operator[](const uint32_t index) { return data_[index]; }

	inline T* data() const { return data_; }

	inline uint32_t size() const { return length_; }

	T* iterator;
	typedef const T* const_iterator;

	inline iterator begin() { return &data_[0]; }
	inline const_iterator begin() const { return &data_[0]; }
	inline iterator end() { return &data_[length_]; }
	inline const_iterator end() const { return &data_[length_]; }
};
#pragma pack(pop)
