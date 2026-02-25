#pragma once

#include "../Common/Arguments.hpp"

namespace aita
{
	using namespace std::chrono_literals;

	enum class Action : uint8_t
	{
		Left = 1,
		Right = 2,
		Jump = 3
	};

	enum class Result : uint8_t
	{
		None = 0,
		Lost,
		Won
	};

	constexpr std::chrono::seconds DefaultEpisodeDuration = 10s;
	constexpr int32_t WindowWidth = 640;
	constexpr int32_t WindowHeight = 480;
	constexpr float StartingPosX = 0.0f;
	constexpr float StartingPosY = 520.0f; // With screen resolution 640x480 this is the start value
	constexpr float MaxScore = std::chrono::duration_cast<std::chrono::milliseconds>(DefaultEpisodeDuration).count();
	constexpr float MinScore = 0.0f;
	constexpr float ProgressReinforcementScore = 1000.0f;
	constexpr float WinFactor = 1.25f;
	constexpr float LossFactor = 0.75f;

	class GameState
	{
	public:
		GameState();
		std::chrono::steady_clock::time_point start;
		float posX;
		float posY;
		float velX;
		float velY;
		std::chrono::milliseconds time;
		float score;
		Result result;

		void reset();
		bool operator == (const GameState& other) const;
		void parse(std::string_view line);

		static std::function<void(const GameState&)> GameOverCallback;

	private:
		void calculateScore();
	};

	constexpr uint64_t DQNStates = 4; // posX, posY, velX, velY
	constexpr uint64_t DQNActions = 3; // left, right, jump

	constexpr std::chrono::seconds DefaultTimeout = std::chrono::hours(1);
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