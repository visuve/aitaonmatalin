#pragma once

namespace aita
{
	enum class Action : uint8_t
	{
		Left = 1,
		Right = 2,
		Jump = 3
	};

	class GameState
	{
	public:
		float posX;
		float posY;
		float velX;
		float velY;

		void reset();

		bool operator == (const GameState& other) const;

		void parse(std::string_view line);
	};
}

std::ostream& operator << (std::ostream& output, aita::GameState& gs);