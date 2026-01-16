#pragma once

#include "RingBuffer.hpp"

namespace aita
{
	class DQN : torch::nn::Module
	{
	public:
		DQN(int64_t states, int64_t actions, int64_t hidden = 128);

		// Returns a pair of q-values and time parameters
		std::pair<torch::Tensor, torch::Tensor> forward(torch::Tensor input);

	private:
		torch::nn::Linear _layer1 = nullptr; // shared feature extractor
		torch::nn::Linear _layer2 = nullptr; // key codes (discrete)
		torch::nn::Linear _layer3 = nullptr; // from-to timings (continuous)
	};
}