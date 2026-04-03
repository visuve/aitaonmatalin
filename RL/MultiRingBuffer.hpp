#pragma once

#include "Logger.hpp"

namespace aita
{
	template<typename T, size_t N>
	class MultiRingBuffer
	{
	public:
		MultiRingBuffer() = delete;

		explicit MultiRingBuffer(size_t size) :
			_size(size)
		{
			if (_size == 0)
			{
				throw std::runtime_error("Size must be greater than zero");
			}

			_indices.fill(0);
			_counts.fill(0);

			for (auto& vec : _data)
			{
				vec.resize(_size);
			}
		}

		template<size_t Index, typename... Args>
		void emplace(Args&&... args)
		{
			static_assert(Index < N, "Buffer index out of bounds");

			size_t& index = _indices[Index];
			size_t& count = _counts[Index];

			_data[Index][index] = T(std::forward<Args>(args)...);

			index = (index + 1) % _size;
			count += (count < _size);
		}

		template<size_t Index>
		void randomSample(std::span<T> samples) const
		{
			static_assert(Index < N, "Buffer index out of bounds");

			const size_t count = _counts[Index];

			if (samples.empty())
			{
				throw std::runtime_error("Resize the sample buffer accordingly");
			}

			if (samples.size() > count)
			{
				throw std::runtime_error("Not enough elements pushed");
			}

			thread_local std::random_device device;
			thread_local std::mt19937 engine(device());

			const auto& vec = _data[Index];

			std::sample(
				vec.begin(),
				vec.begin() + static_cast<std::ptrdiff_t>(count),
				samples.begin(),
				samples.size(),
				engine);
		}

		template<size_t Index>
		size_t count() const
		{
			static_assert(Index < N, "Buffer index out of bounds");
			return _counts[Index];
		}

		size_t size() const
		{
			return _size;
		}

		bool isReadyForBatch(size_t batchSize) const
		{
			const size_t baseSize = batchSize / N;
			const size_t remainder = batchSize % N;

			for (size_t i = 0; i < N; ++i)
			{
				const size_t required = baseSize + (i == N - 1 ? remainder : 0);

				if (_counts[i] < required)
				{
					return false;
				}
			}

			return true;
		}

		void sampleBatch(std::span<T> batch) const
		{
			const size_t baseSize = batch.size() / N;
			const size_t remainder = batch.size() % N;

			size_t offset = 0;

			for (size_t i = 0; i < N; ++i)
			{
				const size_t count = baseSize + (i == N - 1 ? remainder : 0);

				if (count == 0)
				{
					continue;
				}

				thread_local std::random_device device;
				thread_local std::mt19937 engine(device());

				std::sample(
					_data[i].begin(),
					_data[i].begin() + static_cast<std::ptrdiff_t>(_counts[i]),
					batch.begin() + offset,
					count,
					engine);

				offset += count;
			}
		}

		friend std::ostream& operator << (std::ostream& os, const MultiRingBuffer& buffer)
		{
			constexpr size_t elementSize = sizeof(T);
			constexpr size_t bufferCount = N;

			os.write(reinterpret_cast<const char*>(&elementSize), sizeof(elementSize));
			os.write(reinterpret_cast<const char*>(&bufferCount), sizeof(bufferCount));
			os.write(reinterpret_cast<const char*>(&buffer._size), sizeof(buffer._size));

			for (size_t i = 0; i < N; ++i)
			{
				os.write(reinterpret_cast<const char*>(&buffer._indices[i]), sizeof(buffer._indices[i]));
				os.write(reinterpret_cast<const char*>(&buffer._counts[i]), sizeof(buffer._counts[i]));
				os.write(reinterpret_cast<const char*>(buffer._data[i].data()), buffer._size * elementSize);
			}

			return os;
		}

		friend std::istream& operator >> (std::istream& is, MultiRingBuffer& buffer)
		{
			size_t elementSize = 0;
			is.read(reinterpret_cast<char*>(&elementSize), sizeof(elementSize));

			if (elementSize != sizeof(T))
			{
				throw std::runtime_error("MultiRingBuffer element size mismatch");
			}

			size_t bufferCount = 0;
			is.read(reinterpret_cast<char*>(&bufferCount), sizeof(bufferCount));

			if (bufferCount != N)
			{
				throw std::runtime_error("MultiRingBuffer template count (N) mismatch");
			}

			size_t capacity = 0;
			is.read(reinterpret_cast<char*>(&capacity), sizeof(capacity));

			if (capacity != buffer._size)
			{
				throw std::runtime_error("MultiRingBuffer initialized capacity mismatch");
			}

			for (size_t i = 0; i < N; ++i)
			{
				is.read(reinterpret_cast<char*>(&buffer._indices[i]), sizeof(buffer._indices[i]));
				is.read(reinterpret_cast<char*>(&buffer._counts[i]), sizeof(buffer._counts[i]));
				is.read(reinterpret_cast<char*>(buffer._data[i].data()), buffer._size * sizeof(T));
			}

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
		size_t _size;
		std::array<size_t, N> _indices;
		std::array<size_t, N> _counts;
		std::array<std::vector<T>, N> _data;
	};
}