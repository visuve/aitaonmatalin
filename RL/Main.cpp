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

	std::string_view actionToString(int64_t index)
	{
		switch (index)
		{
			case 0:
				return "Left";
			case 1:
				return "Right";
			case 2:
				return "Up";
		}

		throw std::invalid_argument("Unknown action index");
	}

	template <size_t N>
	void loadSession(RingBuffer<Transition<N>>& replayBuffer, Checkpoint& checkpoint)
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

	template <size_t N>
	void saveSession(const RingBuffer<Transition<N>>& replayBuffer, const Checkpoint& checkpoint)
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

	GameState executeActionAndWait(int64_t actionIndex, float delayFloat, float durationFloat)
	{
		const float scaledDuration = MinKeyPressDuration.count() +
			(durationFloat * (MaxKeyPressDuration.count() - MinKeyPressDuration.count()));

		const auto delayTime = std::chrono::milliseconds(static_cast<int>(delayFloat * 1000.0f));
		const auto durationTime = std::chrono::milliseconds(static_cast<int>(scaledDuration));
		const auto actionEndTime = std::chrono::steady_clock::now() + delayTime + durationTime;

		Keyboard keyboard;
		keyboard << KeyPress(keyFromIndex(actionIndex), delayTime, delayTime + durationTime);
		keyboard.sendKeys();

		std::unique_lock<std::mutex> lock(Mutex);

		Condition.wait_until(lock, actionEndTime, []
		{
			return !KeepRunning || GlobalState.result != Result::None;
		});

		return GlobalState;
	}

	template <size_t N>
	struct OptimizationContext
	{
		std::shared_ptr<DQN> network;
		std::shared_ptr<DQN> targetNetwork;
		std::shared_ptr<torch::optim::Optimizer> optimizer;
		RingBuffer<Transition<N>>& replayBuffer;
		std::vector<Transition<N>>& batch;
		const HyperParameters& hp;
	};

	template <size_t N>
	void optimizeNetwork(OptimizationContext<N>& ctx)
	{
		if (ctx.replayBuffer.count() < ctx.hp.batchSize)
		{
			return;
		}

		ctx.replayBuffer.randomSample(ctx.batch);

		const int64_t batchSize = ctx.hp.batchSize;
		torch::Tensor prevStateBatch = torch::empty({ batchSize, static_cast<int64_t>(N) }, torch::kFloat32);
		torch::Tensor nextStateBatch = torch::empty({ batchSize, static_cast<int64_t>(N) }, torch::kFloat32);
		torch::Tensor actionBatch = torch::empty({ batchSize, 1 }, torch::kInt64);
		torch::Tensor rewardBatch = torch::empty({ batchSize }, torch::kFloat32);
		torch::Tensor doneBatch = torch::empty({ batchSize }, torch::kBool);
		torch::Tensor executedTimingsBatch = torch::empty({ batchSize, 2 }, torch::kFloat32);

		for (int64_t i = 0; i < batchSize; ++i)
		{
			const Transition<N>& t = ctx.batch[i];

			std::memcpy(prevStateBatch[i].data_ptr<float>(), t.state.data(), N * sizeof(float));
			std::memcpy(nextStateBatch[i].data_ptr<float>(), t.nextState.data(), N * sizeof(float));

			actionBatch[i][0] = t.action;
			rewardBatch[i] = t.reward;
			doneBatch[i] = t.done;

			executedTimingsBatch[i][0] = t.delay;
			executedTimingsBatch[i][1] = t.duration;
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

		torch::Tensor predictedTimings = torch::empty({ batchSize, 2 }, torch::kFloat32);
		for (int64_t i = 0; i < batchSize; ++i)
		{
			const int64_t idx = actionBatch[i].item<int64_t>() * 2;
			predictedTimings[i][0] = currentTimings[i][idx];
			predictedTimings[i][1] = currentTimings[i][idx + 1];
		}

		torch::Tensor timingLoss = torch::nn::functional::mse_loss(predictedTimings, executedTimingsBatch);

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

		auto network = std::make_shared<DQN>(DQNStates, DQNActions);
		auto targetNetwork = std::make_shared<DQN>(DQNStates, DQNActions);

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

		RingBuffer<Transition<DQNStates>> replayBuffer(hp.replayBufferSize);
		std::vector<Transition<DQNStates>> batch(hp.batchSize);
		Checkpoint checkpoint("aita_dqn.pt", context);
		GameState currentState;

		OptimizationContext<DQNStates> optContext{
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

			const auto [actionIndex, isExploration] = decideAction(currentEpsilon, qValues);
			const int64_t delayIndex = actionIndex * 2;
			const int64_t durationIndex = delayIndex + 1;

			float executedDelay = timings[delayIndex].item<float>();
			float executedDuration = timings[durationIndex].item<float>();

			if (isExploration)
			{
				executedDelay = random(FloatDist);
				executedDuration = random(FloatDist);
			}

			const GameState nextState = executeActionAndWait(actionIndex, executedDelay, executedDuration);

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
				actionIndex,
				executedDelay,
				executedDuration,
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
		ensureForegroundWindow(L"Aita on matalin");
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