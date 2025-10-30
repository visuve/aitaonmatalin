#include "Aita.hpp"

namespace aita
{
	class Arguments
	{
	public:
		Arguments(int argc, char** argv) :
			_arguments(argv + 1, argv + argc)
		{
		}

		template <typename T>
		T get(std::string_view flag, T defaultValue) const
		{
			const std::string& value = find(flag);

			if (value == "not found")
			{
				return defaultValue;
			}

			if constexpr (std::is_same_v<T, float>)
			{
				return std::stof(value);
			}

			static_assert("Unsupported type");
		}

		bool contains(const std::string_view flag) const
		{
			return find(flag) != "not found";
		}

	private:
		std::string find(std::string_view flag) const
		{
			for (const std::string& argument : _arguments)
			{
				if (!argument.starts_with(flag))
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

		std::vector<std::string> _arguments;
	};
}

int main(int argc, char** argv)
{
	aita::Arguments arguments(argc, argv);

	if (arguments.contains("--help") || arguments.contains("-h"))
	{
		std::println("aitaonmatalin - an SFML + LibTorch example game");
		std::println("\noptions:");
		std::println("\t--no-gravity     Disable gravity");
		std::println("\t--width=<value>  Set the window width (default: {})", aita::Configuration::DefaultWindowWidth);
		std::println("\t--height=<value> Set the window height (default: {})", aita::Configuration::DefaultWindowHeight);
		std::println("\t--loop           Run the game in a loop");
		return 0;
	}

	const float windowWidth = arguments.get<float>("--width", aita::Configuration::DefaultWindowWidth);
	const float windowHeight = arguments.get<float>("--height", aita::Configuration::DefaultWindowHeight);

	aita::Game game(windowWidth, windowHeight);

	if (arguments.contains("--no-gravity"))
	{
		game.Config.Gravity = 0.0f;
		std::println("Gravity disabled");
	}

	torch::Device device(torch::kCPU);
	torch::Tensor tensor(torch::rand({ 3, 3 }, device));
	std::cout << tensor << std::endl;

	do
	{
		std::println("Score: {}", game.play());

	} while (game && arguments.contains("--loop"));

	return 0;
}