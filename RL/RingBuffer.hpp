#pragma once

namespace aita
{
	template<typename T>
	class RingBuffer
	{
	public:
		RingBuffer() = delete;

		explicit RingBuffer(size_t size) :
			_size(size)
		{
			if (!_size)
			{
				throw std::runtime_error("RingBuffer size must be greater than zero");
			}

			_data.resize(_size);
		}

		void push(const T& value)
		{
			_data[_index] = value;
			advance();
		}

		void push(T&& value)
		{
			_data[_index] = std::move(value);
			advance();
		}

		template<typename... Args>
		void emplace(Args&&... args)
		{
			_data[_index] = T(std::forward<Args>(args)...);
			advance();
		}

		void randomSample(std::span<T> samples) const
		{
			if (samples.empty())
			{
				throw std::runtime_error("Resize the sample buffer accordingly");
			}

			if (samples.size() > _count)
			{
				throw std::runtime_error("Not enough elements pushed");
			}

			thread_local std::random_device device;
			thread_local std::mt19937 engine(device());

			std::sample(
				_data.begin(),
				_data.begin() + static_cast<std::ptrdiff_t>(_count),
				samples.begin(),
				samples.size(),
				engine);
		}

		inline size_t count() const
		{
			return _count;
		}

		inline size_t size() const
		{
			return _size;
		}

	private:
		inline void advance()
		{
			_index = (_index + 1) % _size;
			_count += (_count < _size);
		}

		const size_t _size = 0;
		size_t _index = 0;
		size_t _count = 0;
		std::vector<T> _data;
	};
}