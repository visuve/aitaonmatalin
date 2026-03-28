#include "AitaEnv.hpp"
#include "Keyboard.hpp"
#include "Process.hpp"
#include "RL.hpp"
#include "RingBuffer.hpp"
#include "Logger.hpp"

namespace aita
{
#ifdef WIN32
	void ensureForegroundWindow(std::wstring_view applicationTitle)
	{
		HWND window = nullptr;

		while (!window)
		{
			LOGI("Waiting for the game window to appear...");
			Sleep(250);
			window = FindWindowW(NULL, L"Aita on matalin - The Fence Jump Game");
		}

		LOGI("Window found!");

		if (!SetForegroundWindow(window))
		{
			LOGW("Failed to set foreground window.");
		}
	}
#endif

	std::mutex Mutex;
	std::condition_variable Condition;
	uint64_t Sequence = 0;
	std::atomic<bool> KeepRunning = true;
	GameState GlobalState;

	torch::Tensor toTensor(const GameState& state)
	{
		return torch::tensor({ state.posX, state.posY, state.velX, state.velY });
	}

	std::array<float, DQNStates> toArray(const GameState& state)
	{
		return { state.posX, state.posY, state.velX, state.velY };
	}

	template <size_t S, size_t K, size_t T>
	void loadSession(bool trainingMode, RingBuffer<Transition<S, K, T>>& replayBuffer, Checkpoint& checkpoint)
	{
		if (checkpoint.load())
		{
			LOGI("Checkpoint loaded");
		}
		else if (trainingMode)
		{
			LOGI("Starting new checkpoint");
		}
		else
		{
			throw std::runtime_error("Failed to load checkpoint. No trained weights available.");
		}

		if (trainingMode)
		{
			if (replayBuffer.load("aita_rb.bin"))
			{
				LOGI("Replay buffer loaded");
			}
			else
			{
				LOGI("Starting new replay buffer");
			}
		}
	}

	template <size_t S, size_t K, size_t T>
	void saveSession(const RingBuffer<Transition<S, K, T>>& replayBuffer, const Checkpoint& checkpoint)
	{
		if (checkpoint.save())
		{
			LOGI("Checkpoint saved");
		}
		else
		{
			LOGE("Failed to save checkpoint");
		}

		if (replayBuffer.save("aita_rb.bin"))
		{
			LOGI("Replay buffer saved");
		}
		else
		{
			LOGE("Failed to save replay buffer");
		}
	}

	void parseGameState(std::string_view processOutput)
	{
		GameState localState;

		try
		{
			localState.parse(processOutput);
		}
		catch (const std::exception& e)
		{
			LOGE("Failed to parse game state from process output: {}. Exception {}", processOutput, e.what());
			return;
		}

		{
			std::lock_guard<std::mutex> lock(Mutex);
			GlobalState = localState;
			++Sequence;
		}

		Condition.notify_all();
	}

	bool observeState(GameState& state)
	{
		std::unique_lock<std::mutex> lock(Mutex);

		const uint64_t currentSequence = Sequence;

		LOGD("Observing...");

		if (!Condition.wait_for(lock, DefaultEpisodeTimeout,
			[&] { return Sequence != currentSequence && GlobalState.result == Result::None; }))
		{
			LOGW("Timeout waiting for game state.");
			return false;
		}

		if (!KeepRunning)
		{
			return false;
		}

		state = GlobalState;
		return true;
	}

	std::pair<std::bitset<DQNKeys>, bool> decideAction(float currentEpsilon, const torch::Tensor& qValues)
	{
		const bool isExploration = random(FloatDist) < currentEpsilon;

		if (isExploration)
		{
			return { random(ActionDist), true };
		}

		return { qValues.argmax().item<int64_t>(), false };
	}

	GameState executeActionAndWait(std::bitset<DQNKeys> actions, const std::array<float, DQNTimings>& timings)
	{
		Keyboard keyboard;
		auto maxEndTime = std::chrono::steady_clock::now();
		bool keysPressed = false;

		for (size_t i = 0; i < DQNKeys; ++i)
		{
			if (actions.test(i))
			{
				keysPressed = true;
				const float delayFloat = timings[i * 2];
				const float durationFloat = timings[i * 2 + 1];

				const float scaledDuration = MinKeyPressDuration.count() +
					(durationFloat * (MaxKeyPressDuration.count() - MinKeyPressDuration.count()));

				const auto delayTime = std::chrono::milliseconds(static_cast<int>(delayFloat * MaxKeyPressDuration.count()));
				const auto durationTime = std::chrono::milliseconds(static_cast<int>(scaledDuration));
				const auto endTime = std::chrono::steady_clock::now() + delayTime + durationTime;

				keyboard << KeyPress(keyFromIndex(i), delayTime, delayTime + durationTime);

				if (endTime > maxEndTime)
				{
					maxEndTime = endTime;
				}
			}
		}

		if (keysPressed)
		{
			keyboard.sendKeys();
		}
		else
		{
			maxEndTime = std::chrono::steady_clock::now() + MinKeyPressDuration;
		}

		std::unique_lock<std::mutex> lock(Mutex);
		Condition.wait_until(lock, maxEndTime, []
		{
			return !KeepRunning || GlobalState.result != Result::None;
		});

		return GlobalState;
	}

	template <size_t S, size_t K, size_t T>
	struct OptimizationContext
	{
		std::shared_ptr<DQN> network;
		std::shared_ptr<DQN> targetNetwork;
		std::shared_ptr<torch::optim::Optimizer> optimizer;
		RingBuffer<Transition<S, K, T>>& replayBuffer;
		std::vector<Transition<S, K, T>>& batch;
		const HyperParameters& hp;
	};

	template <size_t S, size_t K, size_t T>
	void optimizeNetwork(OptimizationContext<S, K, T>& ctx)
	{
		if (ctx.replayBuffer.count() < ctx.hp.batchSize)
		{
			return;
		}

		ctx.replayBuffer.randomSample(ctx.batch);

		const int64_t batchSize = ctx.hp.batchSize;
		torch::Tensor prevStateBatch = torch::empty({ batchSize, static_cast<int64_t>(S) }, torch::kFloat32);
		torch::Tensor nextStateBatch = torch::empty({ batchSize, static_cast<int64_t>(S) }, torch::kFloat32);
		torch::Tensor actionBatch = torch::empty({ batchSize, 1 }, torch::kInt64);
		torch::Tensor rewardBatch = torch::empty({ batchSize }, torch::kFloat32);
		torch::Tensor doneBatch = torch::empty({ batchSize }, torch::kBool);
		torch::Tensor executedTimingsBatch = torch::empty({ batchSize, static_cast<int64_t>(T) }, torch::kFloat32);

		for (size_t i = 0; i < batchSize; ++i)
		{
			const Transition<S, K, T>& t = ctx.batch[i];

			std::memcpy(prevStateBatch[i].data_ptr<float>(), t.state.data(), S * sizeof(float));
			std::memcpy(nextStateBatch[i].data_ptr<float>(), t.nextState.data(), S * sizeof(float));

			actionBatch[i][0] = static_cast<int64_t>(t.action.to_ullong());
			rewardBatch[i] = t.reward;
			doneBatch[i] = t.done;

			std::memcpy(executedTimingsBatch[i].data_ptr<float>(), t.timings.data(), T * sizeof(float));
		}

		auto [currentQValues, currentTimings] = ctx.network->forward(prevStateBatch);
		torch::Tensor stateActionValues = currentQValues.gather(1, actionBatch).squeeze(1);

		torch::Tensor nextStateValues;
		{
			torch::NoGradGuard noGrad;
			auto [nextQValues, _] = ctx.targetNetwork->forward(nextStateBatch);
			nextStateValues = std::get<0>(nextQValues.max(1));
			nextStateValues.masked_fill_(doneBatch, 0.0f);
		}

		torch::Tensor expectedStateActionValues = rewardBatch + (ctx.hp.gamma * nextStateValues);
		torch::Tensor qLoss = torch::nn::functional::smooth_l1_loss(stateActionValues, expectedStateActionValues);

		torch::Tensor mask = torch::zeros({ batchSize, static_cast<int64_t>(T) }, torch::kFloat32);
		for (size_t i = 0; i < batchSize; ++i)
		{
			std::bitset<K> actions(static_cast<uint64_t>(actionBatch[i][0].item<int64_t>()));

			for (size_t k = 0; k < K; ++k)
			{
				if (actions.test(k))
				{
					mask[i][k * 2] = 1.0f;
					mask[i][k * 2 + 1] = 1.0f;
				}
			}
		}

		torch::Tensor timingLoss = torch::nn::functional::mse_loss(
			currentTimings,
			executedTimingsBatch,
			torch::nn::functional::MSELossFuncOptions().reduction(torch::kNone)
		);
		timingLoss = (timingLoss * mask).sum() / mask.sum().clamp_min(1.0f);

		torch::Tensor totalLoss = qLoss + timingLoss;

		ctx.optimizer->zero_grad();
		totalLoss.backward();
		torch::nn::utils::clip_grad_norm_(ctx.network->parameters(), 1.0);
		ctx.optimizer->step();

		{
			torch::NoGradGuard noGrad;
			const float tau = 0.005f;
			auto params = ctx.network->parameters();
			auto targetParams = ctx.targetNetwork->parameters();
			for (size_t i = 0; i < params.size(); ++i)
			{
				targetParams[i].copy_(tau * params[i] + (1.0f - tau) * targetParams[i]);
			}
		}
	}

	void run(bool trainingMode, Process& process, HyperParameters& hp)
	{
		auto network = std::make_shared<DQN>(DQNStates, DQNActions, DQNTimings);
		auto targetNetwork = std::make_shared<DQN>(DQNStates, DQNActions, DQNTimings);

		{
			torch::NoGradGuard noGrad;
			auto params = network->parameters();
			auto targetParams = targetNetwork->parameters();
			for (size_t i = 0; i < params.size(); ++i)
			{
				targetParams[i].copy_(params[i]);
			}
		}

		auto optimizer =
			std::make_shared<torch::optim::Adam>(
				network->parameters(),
				torch::optim::AdamOptions(hp.learningRate));

		float currentEpsilon = trainingMode ? hp.epsilonStart : 0.00f;
		int64_t step = 0;
		int64_t episode = 0;

		TrainingContext context
		{
			network,
			optimizer,
			{
				{ "epsilon", &currentEpsilon },
				{ "step", &step },
				{ "episode", &episode }
			}
		};

		RingBuffer<Transition<DQNStates, DQNKeys, DQNTimings>> replayBuffer(hp.replayBufferSize);
		std::vector<Transition<DQNStates, DQNKeys, DQNTimings>> batch(hp.batchSize);
		Checkpoint checkpoint("aita_dqn.pt", context);
		GameState currentState;

		OptimizationContext<DQNStates, DQNKeys, DQNTimings> optContext{
			network,
			targetNetwork,
			optimizer,
			replayBuffer,
			batch,
			hp
		};

		loadSession(trainingMode, replayBuffer, checkpoint);

		const auto start = std::chrono::steady_clock::now();
		const auto maximumExecTime = start + hp.timeout;
		const auto timeLeft = [&maximumExecTime]()->bool
		{
			return std::chrono::steady_clock::now() < maximumExecTime;
		};

		auto episodeStart = start;

		while (KeepRunning && timeLeft())
		{
			if (!observeState(currentState))
			{
				continue;
			}

			const float currentScore = GameState::calculateScore(currentState, episodeStart);
			const torch::Tensor stateTensor = toTensor(currentState);
			const auto [qValues, timings] = network->forward(stateTensor);
			const auto [actionBitmask, isExploration] = decideAction(currentEpsilon, qValues);

			std::array<float, DQNTimings> executedTimings;

			for (size_t i = 0; i < DQNKeys; ++i)
			{
				if (isExploration)
				{
					executedTimings[i * 2] = random(FloatDist);
					executedTimings[i * 2 + 1] = random(FloatDist);
				}
				else
				{
					executedTimings[i * 2] = timings[i * 2].item<float>();
					executedTimings[i * 2 + 1] = timings[i * 2 + 1].item<float>();
				}
			}

			GameState nextState = executeActionAndWait(actionBitmask, executedTimings);
			const float nextScore = GameState::calculateScore(nextState, episodeStart);
			const float penalty = KeyPressPenalty * actionBitmask.count();
			const float reward = (nextScore - currentScore) - penalty;
			const bool done = (nextState.result != Result::None);

			LOGI("Step {}, Penalty: {:.2f}, Reward: {:.2f}", step, penalty, reward);

			++step;

			if (done)
			{
				++episode;

				LOGI("Episode {} ended. Result: {} | Score: {:.2f} | Steps: {} | Epsilon: {:.4f} | Buffer: {}/{}",
					episode,
					(nextState.result == Result::Won ? "Won" : "Lost"),
					nextScore,
					step,
					currentEpsilon,
					replayBuffer.count(),
					hp.replayBufferSize);

				if (trainingMode && episode % 10 == 0)
				{
					saveSession(replayBuffer, checkpoint);
				}

				episodeStart = std::chrono::steady_clock::now();

				{
					std::lock_guard<std::mutex> lock(Mutex);
					GlobalState.reset();
				}
			}

			if (trainingMode)
			{
				replayBuffer.emplace(
					toArray(currentState),
					actionBitmask,
					executedTimings,
					reward,
					toArray(nextState),
					done);

				optimizeNetwork(optContext);

				if (replayBuffer.count() >= hp.batchSize)
				{
					currentEpsilon = std::max(hp.epsilonMin, currentEpsilon * hp.epsilonDecay);
				}
			}
		}

		if (trainingMode)
		{
			saveSession(replayBuffer, checkpoint);
		}
	}

	BOOL WINAPI consoleHandler(DWORD ctrlType)
	{
		if (ctrlType == CTRL_CLOSE_EVENT)
		{
			KeepRunning = false;
			return TRUE;
		}

		return FALSE;
	}
}

int main(int argc, char** argv)
{
#ifdef WIN32
	SetConsoleCtrlHandler(aita::consoleHandler, TRUE);
#endif

	aita::LOGI("aitaRL");

	try
	{
		using namespace aita;
		using namespace std::chrono_literals;

		Arguments arguments(argc, argv);

		const std::filesystem::path gamePath = arguments.parentPath() / "aitaonmatalin.exe";

		if (!std::filesystem::exists(gamePath))
		{
			throw std::runtime_error("Game executable not found: " + gamePath.string());
		}

		Process process(gamePath, std::format(L"--width={} --height={} --no-sound --loop", WindowWidth, WindowHeight));

		process.start();

#ifdef WIN32
		ensureForegroundWindow(L"Aita on matalin - The Fence Jump Game");
		GlobalState.reset();
		process.redirect(parseGameState);
#endif

		const std::string mode = arguments.get("--mode", "play");

		if (arguments.contains("--example"))
		{
			Keyboard keyboard;
			keyboard
				<< KeyPress(Key::Right, 0ms, 4250ms)
				<< KeyPress(Key::Jump, 1100ms, 1150ms);

			keyboard.sendKeys();
			keyboard.wait();

			std::this_thread::sleep_for(3s);
		}
		else if (mode == "play")
		{
			HyperParameters hp;
			hp.parse(arguments);
			LOGI("Starting in play mode");
			run(false, process, hp);
		}
		else if (mode == "train")
		{
			HyperParameters hp;
			hp.parse(arguments);
			LOGI("Starting in training mode");
			run(true, process, hp);
		}
		else
		{
			LOGE("Bad arguments");
			return ERROR_BAD_ARGUMENTS;
		}

		process.terminate(ERROR_CANCELLED);
		process.waitForExit();

	}
	catch (const std::exception& ex)
	{
		aita::LOGE("An exception occurred: {}", ex.what());
		return -1;
	}

	return 0;
}