#pragma once

namespace aita
{
	class KeyPress
	{
	public:
		KeyPress(uint8_t key, int32_t from, int32_t to);
		void execute(std::stop_source stop_source, std::chrono::steady_clock::time_point then);
	private:
		uint8_t _key;
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
	private:
		std::vector<KeyPress> _keys;
		std::stop_source _stopSource;
		std::vector<std::jthread> _threads;
	};
};
