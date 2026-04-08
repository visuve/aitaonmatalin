#include "Process.hpp"
#include "Logger.hpp"

namespace aita
{
#ifdef WIN32
	Process::Process(const std::filesystem::path& path, const std::vector<std::string>& arguments) :
		_path(path),
		_arguments(arguments)
	{
		SECURITY_ATTRIBUTES sa;
		ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		if (!CreatePipe(_outputReadHandle.addressOf(), _outputWriteHandle.addressOf(), &sa, 0))
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to create output pipe");
		}

		if (!SetHandleInformation(_outputReadHandle, HANDLE_FLAG_INHERIT, 0))
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to set output pipe handle information");
		}
	}

	void Process::start()
	{
		if (_processHandle.isValid())
		{
			throw std::runtime_error("Process is already created.");
		}

		STARTUPINFOA startupInfo;
		ZeroMemory(&startupInfo, sizeof(STARTUPINFOA));
		startupInfo.cb = sizeof(STARTUPINFOA);
		startupInfo.hStdError = _outputWriteHandle;
		startupInfo.hStdOutput = _outputWriteHandle;
		startupInfo.hStdInput = nullptr;
		startupInfo.dwFlags |= STARTF_USESTDHANDLES;

		PROCESS_INFORMATION processInformation;
		ZeroMemory(&processInformation, sizeof(PROCESS_INFORMATION));

		const std::string applicationName = _path.string();
		const auto view = std::views::join_with(_arguments, ' ');
		std::string arguments = applicationName + ' ' + std::ranges::to<std::string>(view);

		if (!CreateProcessA(
			applicationName.c_str(), // Application name
			arguments.data(), // Command line
			nullptr, // Process handle not inheritable
			nullptr, // Thread handle not inheritable
			TRUE, // Inherit handles (for pipes)
			0, // No creation flags
			nullptr, // Use parent's environment
			nullptr, // Use parent's current directory
			&startupInfo, // Startup info
			&processInformation)) // Process information
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to start game process.");
		}

		// Take ownership of the process and thread handles
		_processHandle.reset(processInformation.hProcess);
		_threadHandle.reset(processInformation.hThread);
		_outputWriteHandle.reset();
	}

	void Process::redirectTo(void* where)
	{
		const auto how = [where](std::string_view output)
		{
			if (!WriteFile(where, output.data(), static_cast<DWORD>(output.size()), nullptr, nullptr))
			{
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to write to output");
			}
		};

		return redirect(how);
	}

	std::optional<std::string> Process::read()
	{
		constexpr size_t bufferSize = 0x1000;
		thread_local char buffer[bufferSize];

		DWORD bytesRead = 0;

		if (!ReadFile(_outputReadHandle, buffer, bufferSize - 1, &bytesRead, nullptr))
		{
			const DWORD error = GetLastError();

			if (error == ERROR_BROKEN_PIPE)
			{
				LOGI("Pipe closed");
				return std::nullopt;
			}

			throw std::system_error(static_cast<int>(error), std::system_category(), "Failed to read from process output pipe");
		}

		return std::string(buffer, bytesRead);
	}

	bool Process::isRunning() const
	{
		return exitCode() == STILL_ACTIVE;
	}

	void Process::terminate(int exitCode) const
	{
		if (!TerminateProcess(_processHandle, static_cast<UINT>(exitCode)))
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to terminate process");
		}
	}

	int Process::exitCode() const
	{
		DWORD exitCode = 0;

		if (!GetExitCodeProcess(_processHandle, &exitCode))
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to get process exit code");
		}

		return static_cast<int>(exitCode);
	}

	bool Process::waitForExit() const
	{
		DWORD result = WaitForSingleObject(_processHandle, INFINITE);

		if (result == WAIT_FAILED)
		{
			throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to wait for process exit");
		}

		return result == WAIT_OBJECT_0;
	}
#else
	Process::Process(const std::filesystem::path& path, const std::vector<std::string>& arguments) :
		_path(path),
		_arguments(arguments)
	{
		int pipefd[2];

		if (pipe(pipefd) == -1)
		{
			throw std::system_error(errno, std::system_category(), "Failed to create output pipe");
		}

		_outputReadDescriptor.reset(pipefd[0]);
		_outputWriteDescriptor.reset(pipefd[1]);
	}

	void Process::start()
	{
		if (_pid != -1)
		{
			throw std::runtime_error("Process is already created.");
		}

		_pid = fork();

		switch (_pid)
		{
			case -1:
			{
				throw std::system_error(errno, std::system_category(), "Failed to fork process");
			}
			case 0: // Child process
			{
				if (_outputWriteDescriptor != STDOUT_FILENO)
				{
					dup2(_outputWriteDescriptor, STDOUT_FILENO);
				}

				if (_outputWriteDescriptor != STDERR_FILENO)
				{
					dup2(_outputWriteDescriptor, STDERR_FILENO);
				}

				_outputWriteDescriptor.reset();
				_outputReadDescriptor.reset();

				std::vector<char*> argv;
				std::string applicationName = _path.string();
				argv.push_back(applicationName.data());

				for (auto& s : _arguments)
				{
					argv.push_back(s.data());
				}

				argv.push_back(nullptr);

				execv(applicationName.c_str(), argv.data());

				exit(errno);
			}
			default: // Parent process
			{
				_outputWriteDescriptor.reset();
			}
		}
	}

	void Process::redirectTo(void* where)
	{
		const auto how = [where](std::string_view output)
		{
			if (::write(fileno(reinterpret_cast<FILE*>(where)), output.data(), output.size()) < 0)
			{
				throw std::system_error(errno, std::system_category(), "Failed to write to output");
			}
		};

		return redirect(how);
	}

	std::optional<std::string> Process::read()
	{
		constexpr size_t bufferSize = 0x1000;
		thread_local char buffer[bufferSize];

		const ssize_t bytesRead = ::read(_outputReadDescriptor, buffer, bufferSize - 1);

		switch (bytesRead)
		{
			case -1:
			{
				const int errorCode = errno;

				if (errorCode == EAGAIN || errorCode == EWOULDBLOCK || errorCode == EINTR)
				{
					return "";
				}

				throw std::system_error(errorCode, std::system_category(), "Failed to read from process output pipe");
			}
			case 0:
			{
				LOGI("Pipe closed");
				return std::nullopt;
			}
			default:
			{
				return std::string(buffer, static_cast<size_t>(bytesRead));
			}
		}
	}

	bool Process::isRunning() const
	{
		if (_exited)
		{
			return false;
		}

		int status = 0;
		const pid_t result = waitpid(_pid, &status, WNOHANG);

		switch (result)
		{
			case -1:
			{
				const int errorCode = errno;

				if (errorCode == ECHILD)
				{
					_exited = true;
					_exitCode = -1;
					return false;
				}

				throw std::system_error(errorCode, std::generic_category(), "waitpid failed");
			}
			case 0:
			{
				return true;
			}
			default:
			{
				assert(result == _pid);

				_exited = true;

				if (WIFEXITED(status))
				{
					_exitCode = WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status))
				{
					_exitCode = WTERMSIG(status);
				}
				else
				{
					LOGE("Process exited with unknown status: {}", status);
					_exitCode = -1;
				}

				return false;
			}
		}
	}

	void Process::terminate(int exitCode) const
	{
		if (!isRunning())
		{
			return;
		}

		if (kill(_pid, SIGKILL) == -1)
		{
			throw std::system_error(errno, std::system_category(), "Failed to terminate process");
		}
	}

	int Process::exitCode() const
	{
		if (isRunning())
		{
			throw std::runtime_error("Process is still running. Exit code is not available.");
		}

		return _exitCode;
	}

	bool Process::waitForExit() const
	{
		if (!isRunning())
		{
			return true;
		}

		int status;

		if (waitpid(_pid, &status, 0) == _pid)
		{
			_exited = true;

			if (WIFEXITED(status))
			{
				_exitCode = WEXITSTATUS(status);
			}
			else if (WIFSIGNALED(status))
			{
				_exitCode = WTERMSIG(status);
			}
		}

		return true;
	}
#endif
	void Process::redirect(std::function<void(std::string_view)> how)
	{
		_thread = std::jthread([this, how]()
		{
			try
			{
				while (isRunning())
				{
					const std::optional<std::string> output = read();

					if (!output.has_value())
					{
						break;
					}

					const std::string value = output.value();

					if (value.empty())
					{
						continue;
					}

					how(value);
				}

				LOGI("Process exited with code: {}", exitCode());
			}
			catch (const std::system_error& ex)
			{
				LOGE("System error in process output redirection: {} (code: {})", ex.what(), ex.code().value());
			}
			catch (const std::exception& ex)
			{
				LOGE("Error in process output redirection: {}", ex.what());
			}
		});
	}
}