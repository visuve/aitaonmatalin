#include "../Common/Arguments.hpp"
#include "Process.hpp"
#include "Keyboard.hpp"

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

int main(int argc, char** argv)
{
	puts("aitaRL");

	try
	{
		using namespace aita;
		using namespace std::chrono_literals;

		// std::filesystem::current_path() may not be the same as the executable's path
		const std::filesystem::path gameDir = std::filesystem::path(argv[0]).parent_path();
		const std::filesystem::path gamePath = gameDir / "aitaonmatalin.exe";

		if (!std::filesystem::exists(gamePath))
		{
			throw std::runtime_error("Game executable not found: " + gamePath.string());
		}

		Process process(gamePath, L"--width=640 --height=480 --no-sound --loop");

		process.start();

#ifdef WIN32
		ensureForegroundWindow(L"Aita on matalin");
		process.redirectTo(GetStdHandle(STD_OUTPUT_HANDLE));
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