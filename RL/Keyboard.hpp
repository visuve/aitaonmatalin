#pragma once

namespace aita
{
	enum class Key : uint8_t
	{
		Left = 0,
		Right,
		Jump
	};

	Key keyFromIndex(int64_t index);

	class KeyPress
	{
	public:
		KeyPress(Key key, std::chrono::milliseconds from, std::chrono::milliseconds to);
		void execute(std::stop_source stop_source, std::chrono::steady_clock::time_point then);
	private:
		Key _key;
		std::chrono::milliseconds _from;
		std::chrono::milliseconds _to;
	};

	class Keyboard
	{
	public:
		Keyboard() = default;
		~Keyboard();
		Keyboard& operator << (KeyPress&& key);
		void sendKeys();
		void wait();
	private:
		std::vector<KeyPress> _keys;
		std::stop_source _stopSource;
		std::vector<std::jthread> _threads;
	};
};
