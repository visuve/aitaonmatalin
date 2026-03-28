#include "AitaEnv.hpp"
#include "Logger.hpp"

namespace aita
{
	constexpr std::string_view WonMarker = "won\r\n";
	constexpr std::string_view LostMarker = "lost\r\n";

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

	float GameState::calculateScore(const GameState& state, std::chrono::steady_clock::time_point start)
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		const float base = std::clamp(MaxScore - (float)elapsed, MinScore, MaxScore);
		const float progress = std::clamp(state.posX / WindowWidth, 0.0f, 1.0f) * ProgressReinforcementScore;
		float total = base + progress;

		switch (state.result)
		{
			case Result::Won:
				total *= WinFactor;
				break;
			case Result::Lost:
				total *= LossFactor;
				break;
		}

		return std::clamp(total, MinScore, MaxScore);
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