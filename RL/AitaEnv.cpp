#include "AitaEnv.hpp"

namespace aita
{
	constexpr float StartingPosX = 0.0f;
	constexpr float StartingPosY = 520.0f; // With screen resolution 640x480 this is the start value

	constexpr bool nearEqual(float a, float b, float epsilon = 0.01f)
	{
		return (a > b ? a - b : b - a) <= epsilon;
	}

	void GameState::reset()
	{
		posX = StartingPosX;
		posY = StartingPosY;
		velX = 0.0f;
		velY = 0.0f;
	}

	bool GameState::operator == (const GameState& other) const
	{
		return nearEqual(posX, other.posX) &&
			nearEqual(posY, other.posY) &&
			nearEqual(velX, other.velX) &&
			nearEqual(velY, other.velY);
	}

	void GameState::parse(std::string_view line)
	{
		std::array<float*, 4> targets = { &posX, &posY, &velX, &velY };
		
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

	void HyperParameters::parse(const Arguments& arguments)
	{
		timeout = arguments.get<std::chrono::seconds>("--timeout", DefaultTimeout);
		replayBufferSize = arguments.get<uint32_t>("--replay_buffer_size", DefaultReplayBufferSize);
	}
}

std::ostream& operator<<(std::ostream& output, const aita::GameState& gs)
{
	return output << gs.posX << ' ' << gs.posY << ' ' << gs.velX << ' ' << gs.velY;
}

std::ostream& operator<<(std::ostream& output, const aita::HyperParameters& hp)
{
	output << "Hyper Parameters:\n";
	output << "Timeout: " << hp.timeout.count() << " seconds\n";
	output << "Replay buffer size: " << hp.replayBufferSize << '\n';
	return output;
}
