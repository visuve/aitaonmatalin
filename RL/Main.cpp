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
	GameState State;

	constexpr float VelocityScaleX = 5.0f;
	constexpr float VelocityScaleY = 15.0f;

	inline torch::Tensor toTensor(const GameState& state)
	{
		return torch::tensor({
			state.posX / static_cast<float>(WindowWidth),
			state.posY / static_cast<float>(WindowHeight),
			state.velX / VelocityScaleX,
			state.velY / VelocityScaleY
		});
	}

	inline std::array<float, DQNStates> toArray(const GameState& state)
	{
		return
		{
			state.posX / static_cast<float>(WindowWidth),
			state.posY / static_cast<float>(WindowHeight),
			state.velX / VelocityScaleX,
			state.velY / VelocityScaleY
		};
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
		std::lock_guard<std::mutex> lock(Mutex);

		try
		{
			State.parse(processOutput);
		}
		catch (const std::exception& e)
		{
			LOGE("Failed to parse game state from process output: {}. Exception {}", processOutput, e.what());
			return;
		}
		
		++Sequence;
		Condition.notify_all();
	}

	bool observeState(GameState& state)
	{
		std::unique_lock<std::mutex> lock(Mutex);
		const uint64_t currentSequence = Sequence;

		LOGD("Observing...");

		if (!Condition.wait_for(lock, DefaultEpisodeTimeout, [&] { return Sequence != currentSequence; }))
		{
			LOGW("Timeout waiting for game state.");
			return false;
		}

		if (!KeepRunning)
		{
			return false;
		}

		state = State;
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

		using FloatMs = std::chrono::duration<float, std::milli>;
		const FloatMs range = MaxKeyPressDuration - MinKeyPressDuration;

		for (size_t i = 0; i < DQNKeys; ++i)
		{
			if (actions.test(i))
			{
				keysPressed = true;

				const float delayFloat = timings[i * 2];
				const float durationFloat = timings[i * 2 + 1];

				const auto delayTime = MinKeyPressDuration +
					std::chrono::duration_cast<std::chrono::milliseconds>(range * delayFloat);

				const auto durationTime = MinKeyPressDuration +
					std::chrono::duration_cast<std::chrono::milliseconds>(range * durationFloat);

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
			return !KeepRunning || State.result != Result::None;
		});

		return State;
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
	void sampleStratifiedBatch(
		const RingBuffer<Transition<S, K, T>>& replayBuffer,
		std::vector<Transition<S, K, T>>& batch,
		size_t batchSize)
	{
		const size_t priorityCount = batchSize / 4;
		const size_t uniformCount = batchSize - priorityCount;

		std::span<Transition<S, K, T>> batchSpan(batch);

		replayBuffer.randomSample(batchSpan.subspan(0, uniformCount));

		size_t found = 0;
		size_t attempts = 0;
		const size_t maxAttempts = priorityCount * 100;

		while (found < priorityCount && attempts < maxAttempts)
		{
			attempts++;
			replayBuffer.randomSample(batchSpan.subspan(uniformCount + found, 1));

			const auto& candidate = batch[uniformCount + found];

			if (candidate.reward >= GoalBonus)
			{
				found++;
			}
		}

		if (found < priorityCount)
		{
			replayBuffer.randomSample(batchSpan.subspan(uniformCount + found, priorityCount - found));
		}
	}

	template <size_t S, size_t K, size_t T>
	void optimizeNetwork(OptimizationContext<S, K, T>& ctx)
	{
		if (ctx.replayBuffer.count() < ctx.hp.batchSize)
		{
			return;
		}

		sampleStratifiedBatch(ctx.replayBuffer, ctx.batch, ctx.hp.batchSize);

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

		torch::Tensor rewardWeights = rewardBatch.clamp_min(0.0f).unsqueeze(1);

		timingLoss = (timingLoss * mask * rewardWeights).sum() / (mask * rewardWeights).sum().clamp_min(1.0f);

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

		float epsilon = trainingMode ? hp.epsilonStart : 0.00f;
		int64_t step = 0;
		int64_t episode = 0;

		TrainingContext context
		{
			network,
			optimizer,
			{
				{ "epsilon", &epsilon },
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

		int32_t tick = 0;

		while (KeepRunning && timeLeft())
		{
			if (!observeState(currentState))
			{
				continue;
			}

			bool done = (currentState.result != Result::None);
			float reward = 0.0f;
			GameState nextState = currentState;

			if (done)
			{
				reward = GameState::calculateEpisodeReward(currentState, tick);
			}
			else
			{
				const torch::Tensor stateTensor = toTensor(currentState);
				const auto [qValues, timings] = network->forward(stateTensor);
				const auto [actionBitmask, isExploration] = decideAction(epsilon, qValues);

				std::array<float, DQNTimings> executedTimings;

				constexpr int maxSteps = (MaxKeyPressDuration - MinKeyPressDuration) / KeyPressResolution;

				for (size_t i = 0; i < DQNKeys; ++i)
				{
					const size_t delayIndex = i * 2;
					const size_t durationIndex = delayIndex + 1;

					const float rawDelay = isExploration ? random(FloatDist) : timings[delayIndex].item<float>();
					const float rawDuration = isExploration ? random(FloatDist) : timings[durationIndex].item<float>();

					executedTimings[delayIndex] = std::round(rawDelay * maxSteps) / static_cast<float>(maxSteps);
					executedTimings[durationIndex] = std::round(rawDuration * maxSteps) / static_cast<float>(maxSteps);
				}

				nextState = executeActionAndWait(actionBitmask, executedTimings);
				done = (nextState.result != Result::None);

				++step;
				++tick;

				reward = done ?
					GameState::calculateEpisodeReward(nextState, tick) :
					GameState::calculateStepReward(currentState, nextState, actionBitmask.count());

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
				}
			}

			LOGI("Step {} | X: {:.2f} | Y: {:.2f} | Reward: {:.2f}",
				step,
				nextState.posX,
				nextState.posY,
				reward);

			if (done)
			{
				++episode;

				const auto now = std::chrono::steady_clock::now();
				const auto remaining = std::max(std::chrono::seconds(0),
					std::chrono::duration_cast<std::chrono::seconds>(maximumExecTime - now));

				LOGI("Episode: {} | Result: {} | Score: {:.2f} | Ticks: {} | Epsilon: {:.5f} | Buffer: {:.2f}% | Time Left: {:%T}",
					episode,
					(nextState.result == Result::Won ? "Won" : "Lost"),
					reward,
					tick,
					epsilon,
					(static_cast<float>(replayBuffer.count()) / hp.replayBufferSize) * 100.0f,
					remaining);

				tick = 0;

				{
					std::lock_guard<std::mutex> lock(Mutex);
					State.reset();
				}

				if (trainingMode)
				{
					if (replayBuffer.count() >= hp.batchSize)
					{
						epsilon = std::max(hp.epsilonMin, epsilon - hp.epsilonDecay);
					}

					if (episode % 10 == 0)
					{
						saveSession(replayBuffer, checkpoint);
					}
				}
			}
		}

		if (trainingMode)
		{
			saveSession(replayBuffer, checkpoint);
		}
	}

#ifdef WIN32
	BOOL WINAPI consoleHandler(DWORD ctrlType)
	{
		if (ctrlType == CTRL_CLOSE_EVENT)
		{
			KeepRunning = false;
			return TRUE;
		}

		return FALSE;
	}
#endif
}

int main(int argc, char** argv)
{
#ifdef WIN32
	SetConsoleCtrlHandler(aita::consoleHandler, TRUE);
	constexpr char GameFileName[] = "aitaonmatalin.exe";
#else
	constexpr int ERROR_BAD_ARGUMENTS = EINVAL;
	constexpr int ERROR_CANCELLED = ECANCELED;
	constexpr char GameFileName[] = "aitaonmatalin";
#endif

	aita::LOGI("aitaRL");

	try
	{
		using namespace aita;
		using namespace std::chrono_literals;

		Arguments arguments(argc, argv);

		const std::filesystem::path gamePath = arguments.parentPath() / GameFileName;

		if (!std::filesystem::exists(gamePath))
		{
			throw std::runtime_error("Game executable not found: " + gamePath.string());
		}

		Process process(gamePath,
		{ 
				std::format("--width={}", WindowWidth),
				std::format("--height={}", WindowHeight),
				"--no-sound",
				"--loop"
		});

		process.start();
#ifdef WIN32
		ensureForegroundWindow(L"Aita on matalin - The Fence Jump Game");
#endif
		process.redirect(parseGameState);

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
	catch (const std::system_error& ex)
	{
		aita::LOGE("A system error occurred: {} (code: {})", ex.what(), ex.code().value());
		return ex.code().value();
	}
	catch (const std::exception& ex)
	{
		aita::LOGE("An exception occurred: {}", ex.what());
		return -1;
	}

	return 0;
}