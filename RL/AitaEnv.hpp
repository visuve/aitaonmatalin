#pragma once

#include "../Common/Arguments.hpp"

namespace aita
{
	using namespace std::chrono_literals;

	enum class Result : uint8_t
	{
		None = 0,
		Lost,
		Won
	};

	constexpr std::chrono::seconds DefaultEpisodeDuration = 10s;
	constexpr std::chrono::seconds DefaultEpisodeTimeout = DefaultEpisodeDuration + 1s;
	constexpr int32_t WindowWidth = 640;
	constexpr int32_t WindowHeight = 480;
	constexpr float StartingPosX = 0.0f;
	constexpr float StartingPosY = 520.0f; // With screen resolution 640x480 this is the start value
	constexpr float MaxScore = std::chrono::duration_cast<std::chrono::milliseconds>(DefaultEpisodeDuration).count();
	constexpr float MinScore = 0.0f;
	constexpr std::chrono::milliseconds MinKeyPressDuration = 100ms;
	constexpr std::chrono::milliseconds MaxKeyPressDuration = DefaultEpisodeDuration / 2;

	constexpr int32_t MaxEpisodeSteps = 600;
	constexpr float ProgressWeight = 1000.0f;
	constexpr float GoalBonus = 1000.0f;
	constexpr float KeyPressPenalty = 5.0f;

	class GameState
	{
	public:
		GameState() = default;
		float posX = StartingPosX;
		float posY = StartingPosY;
		float velX = 0.0f;
		float velY = 0.0f;
		Result result = Result::None;

		void reset();
		void parse(std::string_view line);

		static float calculateStepReward(const GameState& current, const GameState& next, float actions);
		static float calculateEpisodeReward(const GameState& state, int32_t steps);
	};

	constexpr uint64_t DQNStates = 4; // posX, posY, velX, velY
	constexpr uint64_t DQNKeys = 3;
	constexpr uint64_t DQNActions = 1 << DQNKeys;
	constexpr int64_t DQNTimings = DQNKeys * 2;

	constexpr std::chrono::seconds DefaultTimeout = std::chrono::hours(1);
	constexpr uint32_t DefaultReplayBufferSize = 10000;
	constexpr float DefaultEpsilonStart = 1.0f;
	constexpr float DefaultEpsilonMin = 0.05f;
	constexpr float DefaultEpsilonDecay = 0.0004f;
	constexpr uint32_t DefaultBatchSize = 128;
	constexpr float DefaultGamma = 0.99f;
	constexpr float DefaultLearningRate = 0.0001f;

	inline std::uniform_real_distribution<float> FloatDist(0.0f, 1.0f);
	inline std::uniform_int_distribution<int64_t> ActionDist(0, DQNActions - 1);

	class HyperParameters
	{
	public:
		std::chrono::seconds timeout = DefaultTimeout; // Maximum execution time
		uint32_t replayBufferSize = DefaultReplayBufferSize; // Maximum number of transitions to store in memory (not bytes)
		float epsilonStart = DefaultEpsilonStart;
		float epsilonMin = DefaultEpsilonMin;
		float epsilonDecay = DefaultEpsilonDecay;
		uint32_t batchSize = DefaultBatchSize;
		float gamma = DefaultGamma;
		float learningRate = DefaultLearningRate;

		void parse(const Arguments&);
	};
}

template <>
struct std::formatter<aita::GameState>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const aita::GameState& gs, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{} {} {} {}",
			gs.posX,
			gs.posY,
			gs.velX,
			gs.velY);
	}
};

template <>
struct std::formatter<aita::HyperParameters>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	auto format(const aita::HyperParameters& hp, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(),
			"Timeout: {} seconds\n"
			"Replay buffer size: {}\n"
			"Epsilon start: {}\n"
			"Epsilon min: {}\n"
			"Epsilon decay: {}\n"
			"Batch size: {}\n"
			"Gamma: {}\n"
			"Learning rate: {}\n",
			hp.timeout.count(),
			hp.replayBufferSize,
			hp.epsilonStart,
			hp.epsilonMin,
			hp.epsilonDecay,
			hp.batchSize,
			hp.gamma,
			hp.learningRate);
	}
};