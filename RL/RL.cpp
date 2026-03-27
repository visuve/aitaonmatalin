#include "RL.hpp"
#include "Logger.hpp"

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

	void Metric::save(torch::serialize::OutputArchive& archive) const
	{
		std::visit([&](auto&& ptr) 
		{
			archive.write(name, torch::tensor(*ptr));

		}, value);
	}

	void Metric::load(torch::serialize::InputArchive& archive) const
	{
		torch::Tensor tensor;

		if (archive.try_read(name, tensor))
		{
			std::visit([&tensor](auto&& ptr) 
			{
				using T = std::decay_t<decltype(*ptr)>;
				*ptr = tensor.item<T>();
			}, value);
		}
	}

	void TrainingContext::save(torch::serialize::OutputArchive& archive) const
	{
		if (network)
		{
			torch::serialize::OutputArchive netArchive;
			network->save(netArchive);
			archive.write("network", netArchive);
		}

		if (optimizer)
		{
			torch::serialize::OutputArchive optArchive;
			optimizer->save(optArchive);
			archive.write("optimizer", optArchive);
		}

		for (const Metric& metric : metrics)
		{
			metric.save(archive);
		}
	}

	void TrainingContext::load(torch::serialize::InputArchive& archive) const
	{
		if (network)
		{
			torch::serialize::InputArchive netArchive;
			if (archive.try_read("network", netArchive))
			{
				network->load(netArchive);
			}
		}

		if (optimizer)
		{
			torch::serialize::InputArchive optArchive;
			if (archive.try_read("optimizer", optArchive))
			{
				optimizer->load(optArchive);
			}
		}

		for (const Metric& metric : metrics)
		{
			metric.load(archive);
		}
	}
	
	Checkpoint::Checkpoint(const std::filesystem::path& path, TrainingContext& context) :
		_path(path),
		_context(context)
	{
	}

	bool Checkpoint::load()
	{
		if (!std::filesystem::exists(_path))
		{
			LOGW("{} does not exist", _path.string());
			return false;
		}

		try
		{
			torch::serialize::InputArchive archive;
			archive.load_from(_path.string());
			_context.load(archive);
			return true;
		}
		catch (const std::exception& e)
		{
			LOGE("Failed to load checkpoint: {}", e.what());
		}

		return false;
	}

	bool Checkpoint::save() const
	{
		try
		{
			torch::serialize::OutputArchive archive;
			_context.save(archive);
			archive.save_to(_path.string());
			return true;
		}
		catch (const std::exception& e)
		{
			LOGE("Failed to save checkpoint: {}", e.what());
		}

		return false;
	}
}