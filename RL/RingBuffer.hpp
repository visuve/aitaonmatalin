#pragma once

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
			os.write(reinterpret_cast<const char*>(&buffer._size), sizeof(buffer._size));
			os.write(reinterpret_cast<const char*>(&buffer._index), sizeof(buffer._index));
			os.write(reinterpret_cast<const char*>(&buffer._count), sizeof(buffer._count));
			os.write(reinterpret_cast<const char*>(buffer._data.data()), buffer._size * sizeof(T));

			return os;
		}

		friend std::istream& operator >> (std::istream& is, RingBuffer& buffer)
		{
			size_t savedSize = 0;
			is.read(reinterpret_cast<char*>(&savedSize), sizeof(savedSize));

			if (savedSize != buffer._size)
			{
				throw std::runtime_error("RingBuffer size mismatch during load");
			}

			is.read(reinterpret_cast<char*>(&buffer._index), sizeof(buffer._index));
			is.read(reinterpret_cast<char*>(&buffer._count), sizeof(buffer._count));
			is.read(reinterpret_cast<char*>(buffer._data.data()), buffer._size * sizeof(T));

			return is;
		}

		void save(const std::filesystem::path& path) const
		{
			std::ofstream file(path, std::ios::binary);

			if (!file)
			{
				std::println(std::cerr, "Failed to save: {}", path);
				return;
			}

			file << *this;
		}

		void load(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);

			if (!file)
			{
				std::println(std::cerr, "Failed to load: {}", path);
				return;
			}

			file >> *this;
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