#pragma once

#include "Handle.hpp"

namespace aita
{
	class Process
	{	
	public:
		Process(const std::filesystem::path& path, const std::vector<std::string>& arguments);

		void start();
		void redirect(std::function<void(std::string_view)> how);
		void redirectTo(void* where);
		std::optional<std::string> read();
		bool isRunning() const;
		void terminate(int) const;
		int exitCode() const;
		bool waitForExit() const;

		inline operator bool() const
		{
			return isRunning();
		}

	private:
		const std::filesystem::path _path;
		std::vector<std::string> _arguments;
		std::jthread _thread;

#ifdef WIN32
		WinHandle _processHandle;
		WinHandle _threadHandle;
		WinHandle _outputReadHandle;
		WinHandle _outputWriteHandle;
#else
		pid_t _pid = -1;
		PosixHandle _outputReadDescriptor;
		PosixHandle _outputWriteDescriptor;
		mutable int _exitCode = 0;
		mutable bool _exited = false;
#endif
	};
}