#pragma once

namespace aita
{
	class Process
	{	
	public:
		Process(const std::filesystem::path& path, const std::wstring& arguments);
		~Process();

		void start();
		void redirect(void* where);
		std::string read();
		bool isRunning() const;
		void terminate(int) const;
		int exitCode() const;
		bool waitForExit(uint32_t milliseconds = 0xFFFFFFFF) const;

	private:
		const std::filesystem::path _path;
		std::wstring _arguments;

#ifdef WIN32
		STARTUPINFOW _startupInfo;
		PROCESS_INFORMATION _processInformation;
		HANDLE _outputReadHandle;
		HANDLE _outputWriteHandle;
#endif
	};
}