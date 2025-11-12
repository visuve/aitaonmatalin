#include "../Common/Arguments.hpp"
#include "Process.hpp"

int main(int argc, char** argv)
{
	puts("aitaRL");

	try
	{
		// std::filesystem::current_path() may not be the same as the executable's path
		const std::filesystem::path gameDir = std::filesystem::path(argv[0]).parent_path();

		std::filesystem::path gamePath = gameDir / "aitaonmatalin.exe";

		if (!std::filesystem::exists(gamePath))
		{
			throw std::runtime_error("Game executable not found: " + gamePath.string());
		}

		aita::Process process(gamePath, L"--width=640 --height=480 --no-sound --loop");

		process.start();
#ifdef WIN32
		process.redirect(GetStdHandle(STD_OUTPUT_HANDLE));
#endif
		process.waitForExit();

	} catch (const std::exception& ex)
	{
		std::cerr << "An exception occurred: " << ex.what() << std::endl;

		return -1;
	}

	return 0;
}