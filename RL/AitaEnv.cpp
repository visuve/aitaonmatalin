#include "AitaEnv.hpp"
#include "Logger.hpp"

namespace aita
{
	constexpr std::string_view WonMarker = "won";
	constexpr std::string_view LostMarker = "lost";

	void GameState::reset()
	{
		posX = StartingPosX;
		posY = StartingPosY;
		velX = 0.0f;
		velY = 0.0f;
		result = Result::None;
	}

	void GameState::parse(std::string_view line)
	{
		if (line.empty())
		{
			LOGD("Received empty line, skipping");
			return;
		}
		
		if (line.starts_with(LostMarker))
		{
			result = Result::Lost;
			return;
		}

		if (line.starts_with(WonMarker))
		{
			result = Result::Won;
			return;
		}

		if (result != Result::None)
		{
			return; // A reset is pending
		}
		
		const std::array<float*, 4> targets = { &posX, &posY, &velX, &velY };
		const char* iter = line.data();
		const char* const end = line.data() + line.size();

		for (float* target : targets)
		{
			while (iter != end && *iter == ' ')
			{
				++iter;
			}

			auto [ptr, ec] = std::from_chars(iter, end, *target);

			if (ec != std::errc())
			{
				throw std::runtime_error("Failed to parse value");
			}

			iter = ptr;
		}
	}

	float GameState::calculateStepReward(const GameState& current, const GameState& next, float actions)
	{
		float stepReward = -1.0f;

		stepReward -= (actions * KeyPressPenalty);

		const float deltaRatio = (next.posX - current.posX) / static_cast<float>(WindowWidth);

		if (deltaRatio > 0.0f)
		{
			stepReward += (deltaRatio * ProgressWeight);
		}

		const int currentZone = static_cast<int>(std::max(0.0f, current.posX) / ZoneWidth);
		const int nextZone = static_cast<int>(std::max(0.0f, next.posX) / ZoneWidth);

		if (nextZone > currentZone)
		{
			stepReward += ZoneBonus;
		}

		return stepReward;
	}

	float GameState::calculateEpisodeReward(const GameState& state, int32_t steps)
	{
		if (state.result == Result::Won)
		{
			const float efficiency = std::max(0, MaxEpisodeSteps - steps) * 5.0f;
			return GoalBonus + efficiency;
		}

		return 0.0f; 
	}

	void HyperParameters::parse(const Arguments& arguments)
	{
		timeout = arguments.get<std::chrono::seconds>("--timeout", DefaultTimeout);
		replayBufferSize = arguments.get<uint32_t>("--replay_buffer_size", DefaultReplayBufferSize);
		epsilonStart = arguments.get<float>("--epsilon_start", DefaultEpsilonStart);
		epsilonMin = arguments.get<float>("--epsilon_min", DefaultEpsilonMin);
		epsilonDecay = arguments.get<float>("--epsilon_decay", DefaultEpsilonDecay);
		batchSize = arguments.get<uint32_t>("--batch_size", DefaultBatchSize);
		gamma = arguments.get<float>("--gamma", DefaultGamma);
		learningRate = arguments.get<double>("--learning_rate", DefaultLearningRate);
	}
}