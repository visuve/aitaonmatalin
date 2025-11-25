#include "RL.hpp"

namespace aita
{
	DQN::DQN(int64_t states, int64_t actions, int64_t hidden) :
		torch::nn::Module("aitaDQN"),
		_layer1(register_module("layer1", torch::nn::Linear(states, hidden))),
		_layer2(register_module("layer2", torch::nn::Linear(hidden, actions))),
		_layer3(register_module("layer3", torch::nn::Linear(hidden, actions * 2)))
	{
	}

	std::pair<torch::Tensor, torch::Tensor> DQN::forward(torch::Tensor input)
	{
		torch::Tensor x = torch::relu(_layer1(input));
		torch::Tensor q = _layer2->forward(x);
		torch::Tensor raw = _layer3->forward(x);
		torch::Tensor time = torch::sigmoid(raw); // Force timing values between 0.0 and 1.0

		return { q, time };
	}
}