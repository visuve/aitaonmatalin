#include "Aita.hpp"
#include "../Common/Arguments.hpp"

namespace aita::snd
{
	bool NoSound = false;
	constexpr float LowNote = 50.0f;
	constexpr float HighNote = 5000.0f;
	constexpr float NoteStep = 1.5f;
	constexpr uint32_t NoteDuration = 60;
	constexpr uint32_t SleepDuration = 250;

	void lose()
	{
		if (NoSound)
		{
			return;
		}

#ifdef WIN32
		for (float note = HighNote; note >= LowNote; note /= NoteStep)
		{
			_beep(static_cast<uint32_t>(note), NoteDuration);
		}
		_sleep(SleepDuration);
#endif
	}

	void win()
	{
		if (NoSound)
		{
			return;
		}

#ifdef WIN32
		for (float note = LowNote; note <= HighNote; note *= NoteStep)
		{
			_beep(static_cast<uint32_t>(note), NoteDuration);
		}
		_sleep(SleepDuration);
#endif
	}
}

int main(int argc, char** argv)
{
	aita::Arguments arguments(argc, argv);

	if (arguments.contains("--help") || arguments.contains("-h"))
	{
		puts("aitaonmatalin - an SFML example game");
		puts("\noptions:");
		puts("\t--no-gravity\t\tDisable gravity");
		printf("\t--width=<value>\t\tSet the window width (default: %.1f)\n", aita::Configuration::DefaultWindowWidth);
		printf("\t--height=<value>\tSet the window height (default: %.1f)\n", aita::Configuration::DefaultWindowHeight);
		puts("\t--loop\t\t\tRun the game in a loop");
		puts("\t--no-sound\t\tDisable sounds");
		return 0;
	}

	const float windowWidth = arguments.get<float>("--width", aita::Configuration::DefaultWindowWidth);
	const float windowHeight = arguments.get<float>("--height", aita::Configuration::DefaultWindowHeight);
	aita::snd::NoSound = arguments.contains("--no-sound");

	aita::Game game(windowWidth, windowHeight);

	if (arguments.contains("--no-gravity"))
	{
		game.Config.Gravity = 0.0f;
		std::println("Gravity disabled");
	}
	
	std::cout << std::setprecision(2) << std::fixed;

	do
	{
		int32_t score = game.play();
		
		if (score)
		{
			std::println("won");
			aita::snd::win();
		}
		else
		{
			std::println("lost");
			aita::snd::lose();
		}

		std::cout.flush();

	} while (game && arguments.contains("--loop"));

	return 0;
}