#include "../Common/Arguments.hpp"
#include "Process.hpp"
#include "Keyboard.hpp"

namespace aita
{
#ifdef WIN32
	void ensureForegroundWindow(std::wstring_view applicationTitle)
	{
		HWND window = nullptr;

		while (!window)
		{
			puts("Waiting for the game window to appear...");
			Sleep(250);
			window = FindWindowW(NULL, L"Aita on matalin");
		}

		puts("Window found!");

		if (!SetForegroundWindow(window))
		{
			puts("Failed to set foreground window.");
		}
	}
#endif
	struct GameState
	{
		float posX;
		float posY;
		float velX;
		float velY;

		void reset()
		{
			posX = 0.0f;
			posY = 520.0f; // With screen resolution 640x480 this is the start value
			velX = 0.0f;
			velY = 0.0f;
		}

		operator torch::Tensor() const
		{
			return torch::tensor({ { posX, posY, velX, velY } }, torch::kFloat32);
		}
	};

	std::istream& operator >> (std::istream& input, GameState& gs)
	{
		return input >> gs.posX >> gs.posY >> gs.velX >> gs.velY;
	}

	std::ostream& operator << (std::ostream& output, GameState& gs)
	{
		return output << gs.posX << ' ' << gs.posY << ' ' << gs.velX << ' ' << gs.velY;
	}

	std::mutex Mutex;
	GameState State;

	void parseGameState(std::string_view processOutput)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		if (processOutput.starts_with("won") || processOutput.starts_with("lost"))
		{
			State.reset();
			return;
		}

		thread_local std::stringstream ss;
		ss << processOutput;
		ss >> State;
		ss.clear();
		std::cout << State << std::endl;
	}
}

int main(int argc, char** argv)
{
	puts("aitaRL");

	try
	{
		using namespace aita;
		using namespace std::chrono_literals;

		Arguments arguments(argc, argv);

		const std::filesystem::path gamePath = arguments.parentPath() / "aitaonmatalin.exe";

		if (!std::filesystem::exists(gamePath))
		{
			throw std::runtime_error("Game executable not found: " + gamePath.string());
		}

		Process process(gamePath, L"--width=640 --height=480 --no-sound --loop");

		process.start();

#ifdef WIN32
		ensureForegroundWindow(L"Aita on matalin");
		process.redirect(parseGameState);
		State.reset();
#endif
		Keyboard keyboard;
		keyboard 
			<< KeyPress(VK_RIGHT, 0ms, 4250ms)
			<< KeyPress(VK_SPACE, 1100ms, 1150ms);

		keyboard.sendKeys();

		process.waitForExit();

	} catch (const std::exception& ex)
	{
		std::cerr << "An exception occurred: " << ex.what() << std::endl;

		return -1;
	}

	return 0;
}