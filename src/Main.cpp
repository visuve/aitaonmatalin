namespace aita
{
	struct GameConfiguration
	{
		constexpr static uint32_t FramesPerSecond = 30;
		constexpr static float DefaultWindowWidth = 800.0f;
		constexpr static float DefaultWindowHeight = 600.0f;

		const float WindowWidth;
		const float WindowHeight;

		const float FenceWidth;
		const float FenceHeight;
		const float FenceHalf;
		const float FenceX;
		const float FenceY;
		const float FenceLeft;
		const float FenceMiddle;
		const float FenceRight;

		float Gravity;
		const float JumpVelocity;
		const float MoveVelocity;
		const float HitPenaltyHorizontal;
		const float HitPenaltyVertical;

		GameConfiguration(float width, float height) :
			WindowWidth(width),
			WindowHeight(height),
			FenceWidth(WindowWidth / 40.0f),
			FenceHeight(WindowHeight / 3.0f),
			FenceHalf(FenceWidth / 2.0f),
			FenceX(WindowWidth / 2 - FenceHalf),
			FenceY(WindowHeight - FenceHeight),
			FenceLeft(FenceX),
			FenceMiddle(FenceLeft + FenceHalf),
			FenceRight(FenceLeft + FenceWidth),
			Gravity(WindowHeight / 1200.0f),
			JumpVelocity(-(WindowHeight / 37.5f)),
			MoveVelocity(WindowWidth / 160.0f),
			HitPenaltyHorizontal(WindowWidth / 53.33f),
			HitPenaltyVertical(WindowHeight / 60.0f)
		{
		}
	};

	namespace snd
	{
		constexpr float LowNote = 50.0f;
		constexpr float HighNote = 5000.0f;
		constexpr float NoteStep = 1.5f;
		constexpr uint32_t NoteDuration = 60;

		void lose()
		{
#ifdef WIN32
			for (float note = HighNote; note >= LowNote; note /= NoteStep)
			{
				_beep(static_cast<uint32_t>(note), NoteDuration);
			}
			_sleep(500);
#endif
		}

		void win()
		{
#ifdef WIN32
			for (float note = LowNote; note <= HighNote; note *= NoteStep)
			{
				_beep(static_cast<uint32_t>(note), NoteDuration);
			}
			_sleep(500);
#endif
		}
	}

	class Player : public sf::Drawable
	{
	public:
		const float Radius;
		const float Diameter;
		const float MinimumX;
		const float MaximumX;
		const float MinimumY;
		const float MaximumY;

		Player(const GameConfiguration& config) :
			Radius((config.WindowWidth + config.WindowHeight) / 35.0f),
			Diameter(Radius * 2.0f),
			MinimumX(0.0f),
			MaximumX(config.WindowWidth - Diameter),
			MinimumY(0.0f),
			MaximumY(config.WindowHeight - Diameter),
			_config(config),
			_shape(Radius, 8),
			_position(MinimumX, MaximumY),
			_velocity(0.0f, 0.0f)
		{
			_shape.setPosition(_position);
			_shape.setFillColor(sf::Color::Green);
		}

		void reset()
		{
			_position = { MinimumX, MaximumY };
			_velocity = { 0.0f, 0.0f };
			_shape.setPosition(_position);
		}

		void jump()
		{
			// Is on ground level
			if (_position.y >= MaximumY)
			{
				_velocity.y = _config.JumpVelocity;
			}
		}

		void move(sf::Vector2f direction)
		{
			_position += direction;
		}

		void update()
		{
			_position += _velocity;

			// Boundary collision / clamping
			if (_position.x < MinimumX)
			{
				_position.x = MinimumX;
			}
			else if (_position.x >= MaximumX)
			{
				_position.x = MaximumX;
			}

			if (_position.y < MinimumY)
			{
				_position.y = MinimumY;
			}
			else if (_position.y >= MaximumY)
			{
				_position.y = MaximumY;
				_velocity.y = 0.0f; // Back to ground
			}
			else
			{
				// Apply gravity
				_velocity.y += _config.Gravity;
			}

			_shape.setPosition(_position);

			const float playerLeft = _position.x;
			const float playerRight = _position.x + Diameter;
			const float playerCenter = _position.x + Radius;

			// Fence collision
			if (_position.y > _config.FenceY - Diameter && playerRight > _config.FenceLeft && playerLeft < _config.FenceRight)
			{
				if (playerCenter < _config.FenceMiddle)
				{
					_shape.setFillColor(sf::Color::Blue);
					_position.x -= _config.HitPenaltyHorizontal;
					_position.y += _config.HitPenaltyVertical;
				}

				if (playerCenter > _config.FenceMiddle)
				{
					_shape.setFillColor(sf::Color::Magenta);
					_position.x += _config.HitPenaltyHorizontal;
					_position.y += _config.HitPenaltyVertical;
				}
			}
			else
			{
				// Higher than fence
				_shape.setFillColor(sf::Color::Green);
			}
		}

		sf::Vector2f bottomRight() const
		{
			return { _position.x + Diameter, _position.y + Diameter };
		}

		void draw(sf::RenderTarget& target, sf::RenderStates states) const override
		{
			target.draw(_shape, states);
		}

	private:
		const aita::GameConfiguration& _config;
		sf::Vector2f _position;
		sf::Vector2f _velocity;
		sf::CircleShape _shape;
	};

	class Game
	{
	public:
		Game() = delete;

		Game(float width, float height) :
			Config(width, height),
			_player(Config)
		{
			sf::Vector2u resolution(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
			sf::VideoMode videoMode(resolution, 8);

			_window = sf::RenderWindow(videoMode, "Aita on matalin");
			_window.setVerticalSyncEnabled(true);
			_window.setFramerateLimit(Config.FramesPerSecond);

			_fence = sf::RectangleShape({ Config.FenceWidth, Config.FenceHeight });
			_fence.setFillColor(sf::Color::Red);
			_fence.setPosition({ Config.FenceX, Config.FenceY });
		}

		GameConfiguration Config;

		operator bool() const
		{
			return _window.isOpen();
		}

		int32_t play()
		{
			const auto handleClose = std::bind(&Game::onClose, this, std::placeholders::_1);
			const auto handleKeypress = std::bind(&Game::onKeyPressed, this, std::placeholders::_1);

			_player.reset();

			// The score is inversely proportional to the time taken to finish the game
			// Higher score equals less time taken to make the jump
			int32_t score = GameConfiguration::FramesPerSecond * 10;

			while (_window.isOpen())
			{
				_window.handleEvents(handleClose, handleKeypress);

				onMove();

				_player.update();

				_window.clear();
				_window.draw(_fence);
				_window.draw(_player);
				_window.display();

				if (_player.bottomRight().x >= Config.WindowWidth &&
					_player.bottomRight().y >= Config.WindowHeight)
				{
					snd::win();
					break;
				}

				if (!--score)
				{
					snd::lose();
					break;
				}
			}

			return score;
		}

	private:
		void onClose(const sf::Event::Closed& closed)
		{
			_window.close();
		}

		void onKeyPressed(const sf::Event::KeyPressed& keyPressed)
		{
			if (keyPressed.scancode == sf::Keyboard::Scancode::Escape)
			{
				_window.close();
			}
			if (keyPressed.scancode == sf::Keyboard::Scancode::Space)
			{
				_player.jump();
			}
		}

		void onMove()
		{
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
			{
				_player.move({ Config.MoveVelocity, 0.0f });
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
			{
				_player.move({ -Config.MoveVelocity, 0.0f });
			}

			if (Config.Gravity > 0.0f)
			{
				return;
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
			{
				_player.move({ 0.0f, -Config.MoveVelocity });
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
			{
				_player.move({ 0.0f, Config.MoveVelocity });
			}
		}

		Player _player;
		sf::RenderWindow _window;
		sf::RectangleShape _fence;
	};

	class Arguments
	{
	public:
		Arguments(int argc, char** argv) :
			_arguments(argv + 1, argv + argc)
		{
		}

		template <typename T>
		T get(std::string_view flag, T defaultValue) const
		{
			const std::string& value = find(flag);

			if (value == "not found")
			{
				return defaultValue;
			}

			if constexpr (std::is_same_v<T, float>)
			{
				return std::stof(value);
			}

			static_assert("Unsupported type");
		}

		bool contains(const std::string_view flag) const
		{
			return find(flag) != "not found";
		}

	private:
		std::string find(std::string_view flag) const
		{
			for (const std::string& argument : _arguments)
			{
				if (!argument.starts_with(flag))
				{
					continue;
				}

				size_t equalSignIndex = argument.find_last_of('=');

				if (equalSignIndex == std::string::npos)
				{
					return argument;
				}

				return argument.substr(++equalSignIndex);
			}

			return "not found";
		}

		std::vector<std::string> _arguments;
	};
}

int main(int argc, char** argv)
{
	aita::Arguments arguments(argc, argv);

	if (arguments.contains("--help") || arguments.contains("-h"))
	{
		std::println("aitaonmatalin - an SFML + LibTorch example game");
		std::println("\noptions:");
		std::println("\t--no-gravity     Disable gravity");
		std::println("\t--width=<value>  Set the window width (default: {})", aita::GameConfiguration::DefaultWindowWidth);
		std::println("\t--height=<value> Set the window height (default: {})", aita::GameConfiguration::DefaultWindowHeight);
		std::println("\t--loop           Run the game in a loop");
		return 0;
	}

	const float windowWidth = arguments.get<float>("--width", aita::GameConfiguration::DefaultWindowWidth);
	const float windowHeight = arguments.get<float>("--height", aita::GameConfiguration::DefaultWindowHeight);

	aita::Game game(windowWidth, windowHeight);

	if (arguments.contains("--no-gravity"))
	{
		game.Config.Gravity = 0.0f;
		std::println("Gravity disabled");
	}

	torch::Device device(torch::kCPU);
	torch::Tensor tensor(torch::rand({ 3, 3 }, device));
	std::cout << tensor << std::endl;

	do
	{
		std::println("Score: {}", game.play());

	} while (game && arguments.contains("--loop"));

	return 0;
}