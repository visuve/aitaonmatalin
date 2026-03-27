#include "Keyboard.hpp"
#include "Logger.hpp"

namespace aita
{
	Key keyFromIndex(int64_t index)
	{
		switch (index)
		{
			case 0:
				return Key::Left;
			case 1:
				return Key::Right;
			case 2:
				return Key::Jump;
		}

		throw std::invalid_argument("Invalid index");
	}

	std::string_view toString(Key key)
	{
		switch (key)
		{
			case Key::Left:
				return "Left";
			case Key::Right:
				return "Right";
			case Key::Jump:
				return "Jump";
		}

		throw std::invalid_argument("Invalid Key");
	}

#ifdef WIN32
	constexpr DWORD KeyDown = 0x0000;
	constexpr DWORD KeyExtended = 0x0001;
	constexpr DWORD KeyUp = 0x0002;

	BYTE toVirtualKey(Key key)
	{
		switch (key)
		{
		case Key::Left:
			return VK_LEFT;
		case Key::Right:
			return VK_RIGHT;
		case Key::Jump:
			return VK_SPACE;
		}

		throw std::invalid_argument("Invalid key");
	}
#endif

	KeyPress::KeyPress(Key key, std::chrono::milliseconds from, std::chrono::milliseconds to) :
		_key(key),
		_from(from),
		_to(to)
	{
		LOGD("{} {} {}", toString(key), from, to);
	}

	void KeyPress::execute(std::stop_source stop_source, std::chrono::steady_clock::time_point startTime)
	{
		std::stop_token token = stop_source.get_token();

		const auto offset = std::chrono::steady_clock::now() - startTime;
		const auto delay = _from - offset;
		const auto duration = _to - _from;

		const auto wait = [&](std::chrono::nanoseconds waitTime)
		{
			if (waitTime <= std::chrono::milliseconds(1))
			{
				return true;
			}

			std::mutex mtx;
			std::unique_lock<std::mutex> lock(mtx);
			std::condition_variable_any cv;

			cv.wait_for(lock, token, waitTime, []
			{
				return false;
			});

			return !token.stop_requested();
		};

#ifdef WIN32
	 	const BYTE key = toVirtualKey(_key);
		const UINT scan = static_cast<BYTE>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC));

		if (!wait(delay))
		{
			return;
		}

		keybd_event(key, scan, KeyDown | KeyExtended, 0);

		wait(duration);

		keybd_event(key, scan, KeyUp | KeyExtended, 0);
		
		LOGD("{} executed", toString(_key));
#endif
	}

	Keyboard::~Keyboard()
	{
		_stopSource.request_stop();
	}

	Keyboard& Keyboard::operator<<(KeyPress&& key)
	{
		_keys.emplace_back(key);
		return *this;
	}

	void Keyboard::sendKeys()
	{
		auto startTime = std::chrono::steady_clock::now();

		for (KeyPress& key : _keys)
		{
			_threads.emplace_back(&KeyPress::execute, &key, _stopSource, startTime);
		}
	}

	void Keyboard::wait()
	{
		for (std::jthread& thread : _threads)
		{
			if (thread.joinable())
			{
				thread.join();
			}
		}
	}
}
