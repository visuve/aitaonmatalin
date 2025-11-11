#include "Aita.hpp"
#include "../Common/Arguments.hpp"

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

	do
	{
		std::println("Score: {}", game.play());

	} while (game && arguments.contains("--loop"));

	return 0;
}