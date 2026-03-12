#include "Keyboard.hpp"

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

#ifdef WIN32
	constexpr DWORD KeyDown = 0x0000;
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
	}

	void KeyPress::execute(std::stop_source stop_source, std::chrono::steady_clock::time_point then)
	{
		std::stop_token token = stop_source.get_token();

		const auto offset = std::chrono::steady_clock::now() - then;

#ifdef WIN32
	 	const BYTE key = toVirtualKey(_key);

		if (token.stop_requested())	{ return; }
		std::this_thread::sleep_for(_from - offset);

		if (token.stop_requested()) { return; }
		keybd_event(key, 0, KeyDown, 0);

		if (token.stop_requested()) { return; }
		std::this_thread::sleep_for(_to - _from);

		if (token.stop_requested()) { return; }
		keybd_event(key, 0, KeyUp, 0);
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
