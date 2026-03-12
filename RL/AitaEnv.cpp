#include "AitaEnv.hpp"

namespace aita
{
	GameState::GameState()
	{
		reset();
	}

	void GameState::reset()
	{
		start = std::chrono::steady_clock::now();
		posX = StartingPosX;
		posY = StartingPosY;
		velX = 0.0f;
		velY = 0.0f;
		time = std::chrono::milliseconds(0);
		score = MaxScore;
		result = Result::None;
	}

	constexpr std::string_view WonMarker = "won\r\n";
	constexpr std::string_view LostMarker = "lost\r\n";

	void GameState::parse(std::string_view line)
	{
		//std::cout << "Parsing line: " << line << std::endl;

		if (line.empty())
		{
			std::cerr << "Received empty line, skipping" << std::endl;
			return;
		}
		
		if (line.starts_with(LostMarker))
		{
			result = Result::Lost;
			line.remove_prefix(LostMarker.size());
			return parse(line);
		}

		if (line.starts_with(WonMarker))
		{
			result = Result::Won;
			line.remove_prefix(WonMarker.size());
			return parse(line);
		}

		calculateScore();
		
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

	void GameState::calculateScore()
	{
		const auto diff = std::chrono::steady_clock::now() - start;
		time = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
		
		const float base = std::clamp(MaxScore - time.count(), MinScore, MaxScore);
		const float progess = std::clamp(posX / WindowWidth, 0.0f, 1.0f) * ProgressReinforcementScore;
		float total = base + progess;

		if (result == Result::Won)
		{
			total *= WinFactor;
		}
		else if (result == Result::Lost)
		{
			total *= LossFactor;
		}

		score = std::clamp(total, MinScore, MaxScore);
		
		if (result != Result::None && GameOverCallback)
		{
			GameOverCallback(*this);
			reset();
		}
	}

	std::function<void(const GameState&)> GameState::GameOverCallback = nullptr;

	void HyperParameters::parse(const Arguments& arguments)
	{
		timeout = arguments.get<std::chrono::seconds>("--timeout", DefaultTimeout);
		replayBufferSize = arguments.get<uint32_t>("--replay_buffer_size", DefaultReplayBufferSize);
		epsilonStart = arguments.get<float>("--epsilon_start", DefaultEpsilonStart);
		epsilonMin = arguments.get<float>("--epsilon_min", DefaultEpsilonMin);
		epsilonDecay = arguments.get<float>("--epsilon_decay", DefaultEpsilonDecay);
	}
}

std::ostream& operator << (std::ostream& output, const aita::GameState& gs)
{
	return output << gs.posX << ' ' << gs.posY << ' ' << gs.velX << ' ' << gs.velY << ' ' << gs.score;
}

std::ostream& operator << (std::ostream& output, const aita::HyperParameters& hp)
{
	output << "Hyper parameters:\n";
	output << "Timeout: " << hp.timeout.count() << " seconds\n";
	output << "Replay buffer size: " << hp.replayBufferSize << '\n';
	output << "Epsilon start: " << hp.epsilonStart << '\n';
	output << "Epsilon min: " << hp.epsilonMin << '\n';
	output << "Epsilon decay: " << hp.epsilonDecay << '\n';
	return output;
}
