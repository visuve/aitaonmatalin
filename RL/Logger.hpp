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
		Logger(Logger&&) = delete;
		Logger& operator=(Logger&&) = delete;

		template <char Level, typename... Args>
		void log(std::format_string<Args...> fmt, Args&&... args)
		{
			std::scoped_lock lock(_mutex);

			auto& stream = []() -> std::ostream&
			{
				if constexpr (Level == 'E' || Level == 'W')
				{
					return std::cerr;
				}

				return std::cout;
			}();

			const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
			const std::chrono::zoned_time localTime(std::chrono::current_zone(), now);

			const std::string message = std::format(fmt, std::forward<Args>(args)...);
			std::println(stream, "[{:%FT%T%z}][{}] {}", localTime, Level, message);

			if (_file)
			{
				std::println(_file, "[{:%FT%T%z}][{}] {}", localTime, Level, message);
			}
		}

	private:
		Logger()
		{
			const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
			const std::chrono::zoned_time localTime(std::chrono::current_zone(), now);
			const std::string fileName = std::format("aita_{:%Y-%m-%d_%H-%M-%S}.log", localTime);

			_file.open(fileName);

			if (!_file)
			{
				throw std::runtime_error(std::format("Failed to open log file: {}", fileName));
			}
		}

		std::mutex _mutex;
		std::ofstream _file;
	};

	inline constexpr struct
	{
#ifndef NDEBUG
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const
		{
			Logger::instance().log<'D'>(fmt, std::forward<Args>(args)...);
		}
#else
		template <typename... Args>
		void operator()(std::format_string<Args...>, Args&&...) const
		{
		}
#endif
	} LOGD;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const 
		{
			Logger::instance().log<'I'>(fmt, std::forward<Args>(args)...);
		}
	} LOGI;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const
		{
			Logger::instance().log<'W'>(fmt, std::forward<Args>(args)...);
		}
	} LOGW;

	inline constexpr struct
	{
		template <typename... Args>
		void operator()(std::format_string<Args...> fmt, Args&&... args) const 
		{
			Logger::instance().log<'E'>(fmt, std::forward<Args>(args)...);
		}
	} LOGE;
}