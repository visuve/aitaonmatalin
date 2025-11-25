#pragma once

namespace aita
{
	class Arguments
	{
	public:
		inline Arguments(int argc, char** argv) :
			_parentPath(std::filesystem::path(argv[0]).parent_path()),
			_arguments(argv + 1, argv + argc)
		{
		}

		template <typename T>
		T get(std::string_view flag, T defaultValue) const
		{
			const std::string value = find(flag);

			if (value == "not found")
			{
				return defaultValue;
			}

			if constexpr (std::is_same_v<T, int32_t>)
			{
				return std::stol(value);
			}

			if constexpr (std::is_same_v<T, int64_t>)
			{
				return std::stoll(value);
			}

			if constexpr (std::is_same_v<T, uint32_t>)
			{
				return std::stoul(value);
			}

			if constexpr (std::is_same_v<T, uint64_t>)
			{
				return std::stoull(value);
			}

			if constexpr (std::is_same_v<T, float>)
			{
				return std::stof(value);
			}

			if constexpr (std::is_same_v<T, double>)
			{
				return std::stod(value);
			}

			static_assert("Unsupported type");
		}

		inline bool contains(const std::string_view flag) const
		{
			return find(flag) != "not found";
		}

		inline std::filesystem::path parentPath() const
		{
			return _parentPath;
		}

	private:
		inline std::string find(std::string_view flag) const
		{
			for (const std::string& argument : _arguments)
			{
				if (!argument.starts_with(flag) || !flag.starts_with(argument))
				{
					continue;
				}

				size_t equalSignIndex = argument.find_last_of('=');

				if (equalSignIndex == std::string::npos)
				{
					return argument;
				}

				return argument.substr(++equalSignIndex);
			}

			return "not found";
		}

		std::filesystem::path _parentPath;
		std::vector<std::string> _arguments;
	};
}