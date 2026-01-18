#include "AitaEnv.hpp"
#include "Keyboard.hpp"
#include "Process.hpp"
#include "RL.hpp"

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

	std::mutex Mutex;
	GameState GlobalState;

	torch::Tensor toTensor(const GameState& state)
	{
		return torch::tensor({ state.posX, state.posY, state.velX, state.velY });
	}

	void parseGameState(std::string_view processOutput)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		if (processOutput.starts_with("won") || processOutput.starts_with("lost"))
		{
			// TODO: implement
			GlobalState.reset();
			return;
		}

		thread_local GameState localState;
		localState.parse(processOutput);

 		if (localState == GlobalState)
		{
			return;
		}

		GlobalState = localState;

		std::cout << GlobalState << std::endl;
	}

	void run(HyperParameters& hp)
	{
		DQN network(DQNStates, DQNActions);
		
		// TODO: implement training and execution loop
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
		GlobalState.reset();
#endif
		if (arguments.contains("--example"))
		{
			Keyboard keyboard;
			keyboard
				<< KeyPress(VK_RIGHT, 0ms, 4250ms)
				<< KeyPress(VK_SPACE, 1100ms, 1150ms);

			keyboard.sendKeys();
		}
		else
		{
			HyperParameters hp;
			hp.parse(arguments);
			run(hp);
		}

		process.waitForExit();

	} catch (const std::exception& ex)
	{
		std::cerr << "An exception occurred: " << ex.what() << std::endl;

		return -1;
	}

	return 0;
}