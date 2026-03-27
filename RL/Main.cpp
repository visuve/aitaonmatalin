#include "AitaEnv.hpp"
#include "Keyboard.hpp"
#include "Process.hpp"
#include "RL.hpp"
#include "RingBuffer.hpp"

namespace aita
{
#ifdef WIN32
	void ensureForegroundWindow(std::wstring_view applicationTitle)
	{
		HWND window = nullptr;

		while (!window)
		{
			std::println("Waiting for the game window to appear...");
			Sleep(250);
			window = FindWindowW(NULL, L"Aita on matalin");
		}

		std::println("Window found!");

		if (!SetForegroundWindow(window))
		{
			std::println("Failed to set foreground window.");
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

	void parseGameState(std::string_view processOutput)
	{
		GameState localState;

		try
		{
			localState.parse(processOutput);
		}
		catch (const std::exception& e)
		{
			std::println(std::cerr, "Failed to parse game state from process output: {}", processOutput);
			std::println(std::cerr, "Exception: {}", e.what());
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

		if (!Condition.wait_for(lock, DefaultEpisodeTimeout,
			[&] { return Sequence != currentSequence && GlobalState.result == Result::None; }))
		{
			std::println(std::cerr, "Timeout waiting for game state.");
			return false;
		}

		if (!KeepRunning)
		{
			return false;
		}

		state = GlobalState;
		return true;
	}

	int64_t decideAction(float currentEpsilon, const torch::Tensor& qValues)
	{
		if (random(FloatDist) < currentEpsilon)
		{
			return random(ActionDist);
		}
		
		return qValues.argmax().item<int64_t>();
	}

	GameState executeActionAndWait(int64_t actionIndex, const torch::Tensor& timings)
	{
		const int64_t delayIndex = actionIndex * 2;
		const int64_t durationIndex = delayIndex + 1;

		const float delayFloat = timings[delayIndex].item<float>();
		const float durationFloat = timings[durationIndex].item<float>();
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
	void loadSession(RingBuffer<Transition<N>>& replayBuffer, Checkpoint& checkpoint)
	{
		if (!checkpoint.load())
		{
			std::println(std::cerr, "Starting new checkpoint");
		}

		if (!replayBuffer.load("replay_buffer.bin"))
		{
			std::println(std::cerr, "Starting new replay buffer");
		}
	}

	template <size_t N>
	void saveSession(const RingBuffer<Transition<N>>& replayBuffer, const Checkpoint& checkpoint)
	{
		if (!checkpoint.save())
		{
			std::println(std::cerr, "Failed to save checkpoint");
		}

		if (!replayBuffer.save("replay_buffer.bin"))
		{
			std::println(std::cerr, "Failed to save replay buffer");
		}
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
			std::println("Game over! Final score: {} Time: {:.3f} Result: {}",
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

		auto optimizer = std::make_shared<torch::optim::Adam>(network->parameters(), torch::optim::AdamOptions(hp.learningRate));

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

			const int64_t actionIndex = decideAction(currentEpsilon, qValues);
			const int64_t delayIndex = actionIndex * 2;
			const int64_t durationIndex = delayIndex + 1;

			const float executedDelay = timings[delayIndex].item<float>();
			const float executedDuration = timings[durationIndex].item<float>();

			const GameState nextState = executeActionAndWait(actionIndex, timings);

			float reward = nextState.score - currentState.score;
			bool done = (nextState.result != Result::None);

			++step;

			if (done)
			{
				++episode;

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

	std::println("aitaRL");

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
			std::cout << hp << std::endl;
		 	run(process, hp);
		}

		process.terminate(1223); // ERROR_CANCELLED
		process.waitForExit();

	}
	catch (const std::exception& ex)
	{
		std::println(std::cerr, "An exception occurred: {}", ex.what());
		return -1;
	}

	return 0;
}