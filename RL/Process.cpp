#include "Process.hpp"

namespace aita
{
	constexpr bool isValid(HANDLE handle)
	{
		return handle != nullptr && handle != INVALID_HANDLE_VALUE;
	}

	Process::Process(const std::filesystem::path& path, const std::wstring& arguments) :
		_path(path),
		_arguments(arguments),
		_outputReadHandle(INVALID_HANDLE_VALUE),
		_outputWriteHandle(INVALID_HANDLE_VALUE)
	{
		SECURITY_ATTRIBUTES sa;
		ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		if (!CreatePipe(&_outputReadHandle, &_outputWriteHandle, &sa, 0))
		{
			throw std::runtime_error("Failed to create output pipe.");
		}

		if (!SetHandleInformation(_outputReadHandle, HANDLE_FLAG_INHERIT, 0))
		{
			throw std::runtime_error("Failed to set output pipe handle information.");
		}

		ZeroMemory(&_startupInfo, sizeof(STARTUPINFOW));
		_startupInfo.cb = sizeof(STARTUPINFOW);
		_startupInfo.hStdError = _outputWriteHandle;
		_startupInfo.hStdOutput = _outputWriteHandle;
		_startupInfo.dwFlags |= STARTF_USESTDHANDLES;

		ZeroMemory(&_processInformation, sizeof(PROCESS_INFORMATION));
	}

	Process::~Process()
	{
		if (isValid(_processInformation.hProcess))
		{
			CloseHandle(_processInformation.hProcess);
		}

		if (isValid(_processInformation.hThread))
		{
			CloseHandle(_processInformation.hThread);
		}

		if (isValid(_outputReadHandle))
		{
			CloseHandle(_outputReadHandle);
		}

		if (isValid(_outputWriteHandle))
		{
			CloseHandle(_outputWriteHandle);
		}
	}

	void Process::start()
	{
		if (isValid(_processInformation.hProcess))
		{
			throw std::runtime_error("Process is already created.");
		}

		if (!CreateProcessW(
			_path.c_str(), // Application name
			_arguments.data(), // Command line
			nullptr, // Process handle not inheritable
			nullptr, // Thread handle not inheritable
			TRUE, // Inherit handles (for pipes)
			0, // No creation flags
			nullptr, // Use parent's environment
			nullptr, // Use parent's current directory
			&_startupInfo, // Startup info
			&_processInformation)) // Process information
		{
			throw std::runtime_error("Failed to start game process.");
		}

		CloseHandle(_outputWriteHandle);
		_outputWriteHandle = INVALID_HANDLE_VALUE;
	}

	void Process::redirect(std::function<void(std::string_view)> how)
	{
		_thread = std::jthread([this, how]()
		{
			while (isRunning())
			{
				std::string output = read();

				if (output.empty())
				{
					continue;
				}

				how(output);
			}
		});
	}

	void Process::redirectTo(void* where)
	{
		const auto how = [where](std::string_view output)
		{
			if (!WriteFile(where, output.data(), static_cast<DWORD>(output.size()), nullptr, nullptr))
			{
				throw std::runtime_error("Failed to write to standard output.");
			}
		};

		return redirect(how);
	}

	std::string Process::read()
	{
		constexpr size_t bufferSize = 0x1000;
		thread_local char buffer[bufferSize];

		DWORD bytesRead = 0;

		if (!ReadFile(_outputReadHandle, buffer, bufferSize - 1, &bytesRead, nullptr))
		{
			throw std::runtime_error("Failed to read from process output pipe.");
		}

		return std::string(buffer, bytesRead);
	}

	bool Process::isRunning() const
	{
		return exitCode() == STILL_ACTIVE;
	}

	void Process::terminate(int exitCode) const
	{
		if (!TerminateProcess(_processInformation.hProcess, static_cast<UINT>(exitCode)))
		{
			throw std::runtime_error("Failed to terminate process.");
		}
	}

	int Process::exitCode() const
	{
		DWORD exitCode = 0;

		if (!GetExitCodeProcess(_processInformation.hProcess, &exitCode))
		{
			throw std::runtime_error("Failed to get process exit code.");
		}

		return static_cast<int>(exitCode);
	}

	bool Process::waitForExit(uint32_t milliseconds) const
	{
		DWORD result = WaitForSingleObject(_processInformation.hProcess, milliseconds);

		if (result == WAIT_FAILED)
		{
			throw std::runtime_error("Failed to wait for process exit.");
		}

		return result == WAIT_OBJECT_0;
	}

}