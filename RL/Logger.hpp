#pragma once

namespace aita
{
	class Logger
	{
	public:
		static Logger& instance()
		{
			static Logger instance;
			return instance;
		}

		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

#if _DEBUG
		template <typename... Args>
		void logDebug(std::format_string<Args...> fmt, Args&&... args)
		{
			std::scoped_lock lock(_mutex);
			logTo(std::cout, 'D', fmt, std::forward<Args>(args)...);
		}
#else
		template <typename... Args>
		void logDebug(std::format_string<Args...>, Args&&...)
		{
		}
#endif

		template <typename... Args>
		void logInfo(std::format_string<Args...> fmt, Args&&... args)
		{
			std::scoped_lock lock(_mutex);
			logTo(std::cout, 'I', fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
		void logWarning(std::format_string<Args...> fmt, Args&&... args)
		{
			std::scoped_lock lock(_mutex);
			logTo(std::cerr, 'W', fmt, std::forward<Args>(args)...);
		}

		template <typename... Args>
		void logError(std::format_string<Args...> fmt, Args&&... args)
		{
			std::scoped_lock lock(_mutex);
			logTo(std::cerr, 'E', fmt, std::forward<Args>(args)...);
		}

	private:
		Logger() = default;
		std::mutex _mutex;

		template <typename... Args>
		void logTo(std::ostream& stream, char level, std::format_string<Args...> fmt, Args&&... args)
		{
			auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
			std::print(stream, "[{:%FT%TZ}][{}] ", now, level);
			std::println(stream, fmt, std::forward<Args>(args)...);
		}
	};

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const
		{
			Logger::instance().logDebug(fmt, std::forward<Args>(args)...);
		}
	} LOGD;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const 
		{
			Logger::instance().logInfo(fmt, std::forward<Args>(args)...);
		}
	} LOGI;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const
		{
			Logger::instance().logWarning(fmt, std::forward<Args>(args)...);
		}
	} LOGW;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const 
		{
			Logger::instance().logError(fmt, std::forward<Args>(args)...);
		}
	} LOGE;

}