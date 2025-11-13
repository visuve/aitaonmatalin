#include "Keyboard.hpp"

namespace aita
{
	constexpr DWORD KeyDown = 0x0000;
	constexpr DWORD KeyUp = 0x0002;

	KeyPress::KeyPress(uint8_t key, int32_t from, int32_t to) :
		_key(key),
		_from(std::chrono::milliseconds(from)),
		_to(std::chrono::milliseconds(to))
	{
	}

	void KeyPress::execute(std::chrono::steady_clock::time_point then)
	{
		const auto offset = std::chrono::steady_clock::now() - then;

		std::this_thread::sleep_for(_from - offset);
		keybd_event(_key, 0, KeyDown, 0);

		std::this_thread::sleep_for(_to - _from);
		keybd_event(_key, 0, KeyUp, 0);
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
			_threads.emplace_back(&KeyPress::execute, &key, startTime);
		}
	}
}
