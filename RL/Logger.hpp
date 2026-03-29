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
			std::print(stream, "[{:%FT%TZ}][{}] ", now, Level);
			std::println(stream, fmt, std::forward<Args>(args)...);
		}

	private:
		Logger() = default;
		std::mutex _mutex;
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