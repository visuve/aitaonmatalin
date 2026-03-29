#pragma once

namespace aita
{
	class Process
	{	
	public:
		Process(const std::filesystem::path& path, const std::vector<std::string>& arguments);
		~Process();

		void start();
		void redirect(std::function<void(std::string_view)> how);
		void redirectTo(void* where);
		std::optional<std::string> read();
		bool isRunning() const;
		void terminate(int) const;
		int exitCode() const;
		bool waitForExit(uint32_t milliseconds = 0xFFFFFFFF) const;

		inline operator bool() const
		{
			return isRunning();
		}

	private:
		const std::filesystem::path _path;
		std::vector<std::string> _arguments;
		std::jthread _thread;

#ifdef WIN32
		STARTUPINFOA _startupInfo;
		PROCESS_INFORMATION _processInformation;
		HANDLE _outputReadHandle;
		HANDLE _outputWriteHandle;
#endif
	};
}