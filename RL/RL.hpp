#pragma once

namespace aita
{
	class DQN : public torch::nn::Module
	{
	public:
		DQN(int64_t states, int64_t actions, int64_t timings, int64_t hidden = 128);

		// Returns a pair of q-values and time parameters
		std::pair<torch::Tensor, torch::Tensor> forward(torch::Tensor input);

	private:
		torch::nn::Linear _layer1 = nullptr; // shared feature extractor
		torch::nn::Linear _layer2 = nullptr; // key codes (discrete)
		torch::nn::Linear _layer3 = nullptr; // from-to timings (continuous)
	};

	struct Metric
	{
		const std::string name;
		std::variant<float*, int64_t*> value;

		void save(torch::serialize::OutputArchive& archive) const;
		void load(torch::serialize::InputArchive& archive) const;
	};

	struct TrainingContext
	{
		std::shared_ptr<torch::nn::Module> network;
		std::shared_ptr<torch::optim::Optimizer> optimizer;
		std::vector<Metric> metrics;

		void save(torch::serialize::OutputArchive& archive) const;
		void load(torch::serialize::InputArchive& archive) const;
	};

	class Checkpoint
	{
	public:
		Checkpoint(const std::filesystem::path& path, TrainingContext& context);
		
		bool load();
		bool save() const;

	private:
		const std::filesystem::path _path;
		TrainingContext& _context;
	};

	template <typename T>
	auto random(T&& distribution)
	{
		thread_local std::random_device device;
		thread_local std::default_random_engine engine(device());
		return distribution(engine);
	}

	template <size_t States, size_t Keys, size_t Timings>
	struct Transition
	{
		std::array<float, States> state;
		std::bitset<Keys> action;
		std::array<float, Timings> timings;
		float reward;
		std::array<float, States> nextState;
		bool done;
	};
}