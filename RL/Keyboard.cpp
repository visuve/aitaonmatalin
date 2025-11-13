#include "Keyboard.hpp"

namespace aita
{
	constexpr DWORD KeyDown = 0x0000;
	constexpr DWORD KeyUp = 0x0002;

	KeyPress::KeyPress(uint8_t key, std::chrono::milliseconds from, std::chrono::milliseconds to) :
		_key(key),
		_from(from),
		_to(to)
	{
	}

	void KeyPress::execute(std::stop_source stop_source, std::chrono::steady_clock::time_point then)
	{
		std::stop_token token = stop_source.get_token();

		const auto offset = std::chrono::steady_clock::now() - then;

		if (token.stop_requested())	{ return; }
		std::this_thread::sleep_for(_from - offset);

		if (token.stop_requested()) { return; }
		keybd_event(_key, 0, KeyDown, 0);

		if (token.stop_requested()) { return; }
		std::this_thread::sleep_for(_to - _from);

		if (token.stop_requested()) { return; }
		keybd_event(_key, 0, KeyUp, 0);
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
}
