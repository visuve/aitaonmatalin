#pragma once

#include "Logger.hpp"

namespace aita
{
	template<typename T> requires std::is_trivially_copyable_v<T>
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

		size_t count() const
		{
			return _count;
		}

		size_t size() const
		{
			return _size;
		}

		friend std::ostream& operator << (std::ostream& os, const RingBuffer& buffer)
		{
			constexpr size_t elementSize = sizeof(T);
			os.write(reinterpret_cast<const char*>(&elementSize), sizeof(elementSize));
			os.write(reinterpret_cast<const char*>(&buffer._size), sizeof(buffer._size));
			os.write(reinterpret_cast<const char*>(&buffer._index), sizeof(buffer._index));
			os.write(reinterpret_cast<const char*>(&buffer._count), sizeof(buffer._count));
			os.write(reinterpret_cast<const char*>(buffer._data.data()), buffer._size * elementSize);

			return os;
		}

		friend std::istream& operator >> (std::istream& is, RingBuffer& buffer)
		{
			size_t elementSize = 0;
			is.read(reinterpret_cast<char*>(&elementSize), sizeof(elementSize));

			if (elementSize != sizeof(T))
			{
				throw std::runtime_error("RingBuffer element size mismatch");
			}

			size_t elementCount = 0;
			is.read(reinterpret_cast<char*>(&elementCount), sizeof(elementCount));

			if (elementCount != buffer._size)
			{
				throw std::runtime_error("RingBuffer element count mismatch");
			}

			is.read(reinterpret_cast<char*>(&buffer._index), sizeof(buffer._index));
			is.read(reinterpret_cast<char*>(&buffer._count), sizeof(buffer._count));
			is.read(reinterpret_cast<char*>(buffer._data.data()), buffer._size * sizeof(T));

			return is;
		}

		bool load(const std::filesystem::path& path)
		{
			if (!std::filesystem::exists(path))
			{
				LOGW("{} does not exist", path.string());
				return false;
			}

			std::ifstream file(path, std::ios::binary);

			if (!file)
			{
				LOGE("Failed to load: {}", path.string());
				return false;
			}

			file >> *this;
			return true;
		}

		bool save(const std::filesystem::path& path) const
		{
			std::ofstream file(path, std::ios::binary);

			if (!file)
			{
				LOGE("Failed to save: {}", path.string());
				return false;
			}

			file << *this;
			return true;
		}

	private:
		void advance()
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