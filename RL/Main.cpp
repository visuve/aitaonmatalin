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
			std::println("Waiting for the game window to appear...");
			Sleep(250);
			window = FindWindowW(NULL, L"Aita on matalin");
		}

		std::println("Window found!");

		if (!SetForegroundWindow(window))
		{
			std::println("Failed to set foreground window.");
		}
	}
#endif

	std::mutex Mutex;
	std::binary_semaphore Semaphore(0);
	std::atomic<bool> KeepRunning = true;
	GameState GlobalState;

	torch::Tensor toTensor(const GameState& state)
	{
		return torch::tensor({ state.posX, state.posY, state.velX, state.velY });
	}

	void parseGameState(std::string_view processOutput)
	{
		thread_local GameState localState;

		try
		{
			localState.parse(processOutput);
		}
		catch (const std::exception& e)
		{
			std::cout << "Failed to parse game state from process output: " << processOutput << std::endl;
			std::cout << "Exception: " << e.what() << std::endl;
			return;
		}

		{
			std::lock_guard<std::mutex> lock(Mutex);

			if (localState == GlobalState)
			{
				return;
			}

			GlobalState = localState;
		}

		Semaphore.release();
	}

	void run(Process& process, HyperParameters& hp)
	{
		GameState::GameOverCallback = [](const GameState& state)
		{
			std::cout << "Game over! Final score: " << state.score
				<< " Time: " << float(state.time.count() / 1000.0f)
				<< " Result: " << (state.result == Result::Won ? "Won" : "Lost") << std::endl;
		};

		DQN network(DQNStates, DQNActions);
		
		const auto start = std::chrono::steady_clock::now();
		const auto maximumExecTime = start + hp.timeout;
		const auto timeLeft = [&maximumExecTime]()->bool
		{
			return std::chrono::steady_clock::now() < maximumExecTime;
		};

		while (KeepRunning && timeLeft())
		{
			if (!Semaphore.try_acquire_for(DefaultEpisodeDuration + 1s))
			{
				std::cerr << "Failed to acquire semaphore within timeout." << std::endl;
				continue;
			}

			std::lock_guard<std::mutex> lock(Mutex);
			std::cout << GlobalState << std::endl;
		}
	}

	BOOL WINAPI consoleHandler(DWORD ctrlType)
	{
		if (ctrlType == CTRL_CLOSE_EVENT)
		{
			// Return TRUE to signal that we've handled the event.
			// This stops the OS from calling the next handler (Intel's crash handler).
			// which ironically causes crash on exit (in my use case)
			KeepRunning = false;
			return TRUE;
		}

		return FALSE;
	}
}

int main(int argc, char** argv)
{
#ifdef WIN32
	SetConsoleCtrlHandler(aita::consoleHandler, TRUE);
#endif

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

		Process process(gamePath, std::format(L"--width={} --height={} --no-sound --loop", WindowWidth, WindowHeight));

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
		 	run(process, hp);
		}

		process.waitForExit();

	} 
	catch (const std::exception& ex)
	{
		std::cerr << "An exception occurred: " << ex.what() << std::endl;

		return -1;
	}

	return 0;
}