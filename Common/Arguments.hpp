#pragma once

namespace aita
{
	template <typename T>
	concept IsDuration = requires { typename T::rep; typename T::period; };

	template <typename T>
	concept IsArithmetic = std::is_arithmetic_v<T>;

	class Arguments
	{
	public:
		inline Arguments(int argc, char** argv) :
			_parentPath(std::filesystem::path(argv[0]).parent_path()),
			_arguments(argv + 1, argv + argc)
		{
		}

		template <IsArithmetic T>
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

		template <IsDuration T>
		T get(std::string_view flag, T defaultValue) const
		{
			return T(get<typename T::rep>(flag, defaultValue.count()));
		}

		std::string get(std::string_view flag, const std::string& defaultValue) const
		{
			const std::string value = find(flag);

			if (value == "not found")
			{
				return defaultValue;
			}

			return value;
		}

		inline bool contains(const std::string_view key) const
		{
			return find(key) != "not found";
		}

		inline std::filesystem::path parentPath() const
		{
			return _parentPath;
		}

	private:
		inline std::string find(std::string_view key) const
		{
			for (const std::string& argument : _arguments)
			{
				if (argument == key)
				{
					return argument;
				}

				size_t equalSignIndex = argument.find_first_of('=');

				if (equalSignIndex == std::string::npos)
				{
					continue;
				}

				const std::string_view parsedKey = std::string_view(argument).substr(0, equalSignIndex);

				if (parsedKey != key)
				{
					continue;
				}

				const std::string parsedValue = argument.substr(++equalSignIndex);

				return parsedValue;
			}

			return "not found";
		}

		std::filesystem::path _parentPath;
		std::vector<std::string> _arguments;
	};
}