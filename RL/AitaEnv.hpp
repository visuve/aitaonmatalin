#pragma once

#include "../Common/Arguments.hpp"

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

	constexpr uint64_t DQNStates = 4; // posX, posY, velX, velY
	constexpr uint64_t DQNActions = 3; // left, right, jump

	constexpr std::chrono::seconds DefaultTimeout = std::chrono::seconds(3600); // 1 hour
	constexpr uint32_t DefaultReplayBufferSize = 100000;

	class HyperParameters
	{
	public:
		std::chrono::seconds timeout = DefaultTimeout;      // Maximum execution time
		uint32_t replayBufferSize = DefaultReplayBufferSize; // Maximum number of transitions to store in memory (not bytes)

		void parse(const Arguments&);
	};
}

std::ostream& operator << (std::ostream& output, const aita::GameState& gs);
std::ostream& operator << (std::ostream& output, const aita::HyperParameters& hp);