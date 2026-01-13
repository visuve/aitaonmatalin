#pragma once

#include <algorithm>
#include <random>
#include <stdexcept>

template<typename T, size_t N>
class RingBuffer
{
public:
	void push(const T& value)
	{
		_data[_index] = value;
		_index = (_index + 1) % N;
		_count += (_count < N);
	}

	T* sample(size_t sampleSize) const
	{
		if (sampleSize > _count)
		{
			throw std::runtime_error("Not enough elements pushed");
		}

		T* result = new T[sampleSize];
		
		thread_local std::random_device device;
		thread_local std::mt19937 engine(device);

		std::sample(
			_data,
			_data + _count,
			result,
			sampleSize,
			engine
		);

		return result;
	}

private:
	size_t _index = 0;
	size_t _count = 0;
	T _data[N];
};