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

	std::string actionToString(int64_t bitmask)
	{
		static const std::array<std::string, 8> ActionStrings = {
			"None",
			"Left",
			"Right",
			"Left + Right",
			"Jump",
			"Left + Jump",
			"Right + Jump",
			"Left + Right + Jump"
		};

		if (bitmask >= 0 && bitmask < ActionStrings.size())
		{
			return ActionStrings[bitmask];
		}

		return "Unknown";
	}

	template <size_t S, size_t T>
	void loadSession(RingBuffer<Transition<S, T>>& replayBuffer, Checkpoint& checkpoint)
	{
		if (checkpoint.load())
		{
			LOGI("Checkpoint loaded");
		}
		else
		{
			LOGI("Starting new checkpoint");
		}

		if (replayBuffer.load("aita_rb.bin"))
		{
			LOGI("Replay buffer loaded");
		}
		else
		{
			LOGI("Starting new replay buffer");
		}
	}

	template <size_t S, size_t T>
	void saveSession(const RingBuffer<Transition<S, T>>& replayBuffer, const Checkpoint& checkpoint)
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

	std::pair<int64_t, bool> decideAction(float currentEpsilon, const torch::Tensor& qValues)
	{
		const bool isExploration = random(FloatDist) < currentEpsilon;

		if (isExploration)
		{
			return { random(ActionDist), true };
		}

		return { qValues.argmax().item<int64_t>(), false };
	}

	GameState executeActionAndWait(int64_t actionBitmask, const std::array<float, DQNTimings>& timings)
	{
		Keyboard keyboard;
		auto maxEndTime = std::chrono::steady_clock::now();
		bool keysPressed = false;

		for (size_t i = 0; i < DQNKeys; ++i)
		{
			if ((actionBitmask & (1 << i)) != 0)
			{
				keysPressed = true;
				const float delayFloat = timings[i * 2];
				const float durationFloat = timings[i * 2 + 1];

				const float scaledDuration = MinKeyPressDuration.count() +
					(durationFloat * (MaxKeyPressDuration.count() - MinKeyPressDuration.count()));

				const auto delayTime = std::chrono::milliseconds(static_cast<int>(delayFloat * 1000.0f));
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
			maxEndTime = std::chrono::steady_clock::now() + 100ms;
		}

		std::unique_lock<std::mutex> lock(Mutex);
		Condition.wait_until(lock, maxEndTime, []
		{
			return !KeepRunning || GlobalState.result != Result::None;
		});

		return GlobalState;
	}

	template <size_t S, size_t T>
	struct OptimizationContext
	{
		std::shared_ptr<DQN> network;
		std::shared_ptr<DQN> targetNetwork;
		std::shared_ptr<torch::optim::Optimizer> optimizer;
		RingBuffer<Transition<S, T>>& replayBuffer;
		std::vector<Transition<S, T>>& batch;
		const HyperParameters& hp;
	};

	template <size_t S, size_t T>
	void optimizeNetwork(OptimizationContext<S, T>& ctx)
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
			const Transition<S, T>& t = ctx.batch[i];

			std::memcpy(prevStateBatch[i].data_ptr<float>(), t.state.data(), S * sizeof(float));
			std::memcpy(nextStateBatch[i].data_ptr<float>(), t.nextState.data(), S * sizeof(float));

			actionBatch[i][0] = t.action;
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
		for (int64_t i = 0; i < batchSize; ++i)
		{
			int64_t action = actionBatch[i][0].item<int64_t>();

			for (size_t k = 0; k < DQNKeys; ++k)
			{
				if ((action & (1 << k)) != 0)
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

	void run(Process& process, HyperParameters& hp)
	{
		GameState::GameOverCallback = [](const GameState& state)
		{
			LOGI("Game over! Final score: {} Time: {:.3f} Result: {}",
				state.score,
				state.time.count() / 1000.0f,
				state.result == Result::Won ? "Won" : "Lost");
		};

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

		float currentEpsilon = hp.epsilonStart;
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

		RingBuffer<Transition<DQNStates, DQNTimings>> replayBuffer(hp.replayBufferSize);
		std::vector<Transition<DQNStates, DQNTimings>> batch(hp.batchSize);
		Checkpoint checkpoint("aita_dqn.pt", context);
		GameState currentState;

		OptimizationContext<DQNStates, DQNTimings> optContext{
			network,
			targetNetwork,
			optimizer,
			replayBuffer,
			batch,
			hp
		};

		loadSession(replayBuffer, checkpoint);

		const auto start = std::chrono::steady_clock::now();
		const auto maximumExecTime = start + hp.timeout;
		const auto timeLeft = [&maximumExecTime]()->bool
		{
			return std::chrono::steady_clock::now() < maximumExecTime;
		};

		while (KeepRunning && timeLeft())
		{
			if (!observeState(currentState))
			{
				continue;
			}

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

			LOGD("Step {}: Action [{}] chosen", step, actionToString(actionBitmask));

			const GameState nextState = executeActionAndWait(actionBitmask, executedTimings);

			float reward = (nextState.score - currentState.score) - KeyPressPenalty;
			bool done = (nextState.result != Result::None);

			++step;

			if (done)
			{
				++episode;

				LOGI("Episode {} ended. Steps: {} | Epsilon: {:.4f} | Buffer: {}/{}",
					episode, step, currentEpsilon, replayBuffer.count(), hp.replayBufferSize);

				if (episode % 10 == 0)
				{
					saveSession(replayBuffer, checkpoint);
				}
			}

			replayBuffer.emplace(
				toArray(currentState),
				actionBitmask,
				executedTimings,
				reward,
				toArray(nextState),
				done
			);

			optimizeNetwork(optContext);

			if (replayBuffer.count() >= hp.batchSize)
			{
				currentEpsilon = std::max(hp.epsilonMin, currentEpsilon * hp.epsilonDecay);
			}
		}

		saveSession(replayBuffer, checkpoint);
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
		else
		{
			HyperParameters hp;
			hp.parse(arguments);
			LOGI("Hyper parameters:\n{}", hp);
			run(process, hp);
		}

		process.terminate(1223); // ERROR_CANCELLED
		process.waitForExit();

	}
	catch (const std::exception& ex)
	{
		aita::LOGE("An exception occurred: {}", ex.what());
		return -1;
	}

	return 0;
}