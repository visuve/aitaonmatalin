namespace aita
{
	constexpr float WindowWidth = 1600.0f;
	constexpr float WindowHeight = 1200.0f;

	constexpr float FenceWidth = WindowWidth / 40.0f;
	constexpr float FenceHeight = WindowHeight / 3.0f;
	constexpr float FenceHalf = FenceWidth / 2.0f;
	constexpr float FenceX = WindowWidth / 2 - FenceHalf;
	constexpr float FenceY = WindowHeight - FenceHeight;
	constexpr float FenceLeft = FenceX;
	constexpr float FenceMiddle = FenceLeft + FenceHalf;
	constexpr float FenceRight = FenceLeft + FenceWidth;

	float Gravity = WindowHeight / 1200.0f;
	constexpr float JumpVelocity = -(WindowHeight / 37.5f);
	constexpr float MoveVelocity = WindowWidth / 160.0f;
	constexpr float HitPenaltyHorizontal = WindowWidth / 53.33f;
	constexpr float HitPenaltyVertical = WindowHeight / 60.0f;

	class Player : public sf::Drawable
	{
	public:
		static constexpr float Radius = (WindowWidth + WindowHeight) / 35.0f;
		static constexpr float Diameter = Radius * 2.0f;
		static constexpr float MinimumX = 0.0f;
		static constexpr float MaximumX = WindowWidth - Diameter;
		static constexpr float MinimumY = 0.0f;
		static constexpr float MaximumY = WindowHeight - Diameter;

		Player() :
			_shape(Radius, 8),
			_position(MinimumX, MaximumY),
			_velocity(0.0f, 0.0f)
		{
			_shape.setPosition(_position);
			_shape.setFillColor(sf::Color::Green);
		}

		~Player() = default;

		void jump()
		{
			// Is on ground level
			if (_position.y >= MaximumY)
			{
				_velocity.y = JumpVelocity;
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
				_velocity.y += Gravity;
			}

			_shape.setPosition(_position);

			const float playerLeft = _position.x;
			const float playerRight = _position.x + Diameter;
			const float playerCenter = _position.x + Radius;

			// Fence collision
			if (_position.y > FenceY - Diameter && playerRight > FenceLeft && playerLeft < FenceRight)
			{
				if (playerCenter < FenceMiddle)
				{
					_shape.setFillColor(sf::Color::Blue);
					_position.x -= HitPenaltyHorizontal;
					_position.y += HitPenaltyVertical;
				}

				if (playerCenter > FenceMiddle)
				{
					_shape.setFillColor(sf::Color::Magenta);
					_position.x += HitPenaltyHorizontal;
					_position.y += HitPenaltyVertical;
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

	private:
		void draw(sf::RenderTarget& target, sf::RenderStates states) const override
		{
			target.draw(_shape, states);
		}

		sf::Vector2f _position;
		sf::Vector2f _velocity;
		sf::CircleShape _shape;
	};

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

int main(int argc, char** argv)
{
	aita::Player player;

	if (argc > 1 && std::string(argv[1]) == "--no-gravity")
	{
		aita::Gravity = 0.0f;
	}

	sf::VideoMode videoMode(
	{
		static_cast<uint32_t>(aita::WindowWidth),
		static_cast<uint32_t>(aita::WindowHeight)
	}, 8);

	sf::RenderWindow window(videoMode, "Aita on matalin");

	window.setVerticalSyncEnabled(true);
	window.setFramerateLimit(30);

	const auto onClose = [&window](const sf::Event::Closed&)
	{
		window.close();
	};

	const auto onKeyPressed = [&window, &player](const sf::Event::KeyPressed& keyPressed)
	{
		if (keyPressed.scancode == sf::Keyboard::Scancode::Escape)
		{
			window.close();
		}

		if (keyPressed.scancode == sf::Keyboard::Scancode::Space)
		{
			player.jump();
		}
	};

	sf::RectangleShape fence({ aita::FenceWidth, aita::FenceHeight });
	fence.setFillColor(sf::Color::Red);
	fence.setPosition({ aita::FenceX, aita::FenceY });

	size_t ticks = 0;

	while (window.isOpen())
	{
		window.handleEvents(onClose, onKeyPressed);

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
		{
			player.move({ aita::MoveVelocity, 0.0f });
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
		{
			player.move({ -aita::MoveVelocity, 0.0f });
		}

		if (aita::Gravity == 0.0f)
		{
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
			{
				player.move({ 0.0f, -aita::MoveVelocity });
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
			{
				player.move({ 0.0f, aita::MoveVelocity });
			}
		}

		player.update();

		window.clear();
		window.draw(fence);
		window.draw(player);
		window.display();

		if (player.bottomRight().x >= aita::WindowWidth &&
			player.bottomRight().y >= aita::WindowHeight)
		{
			aita::win();
			return ticks;
		}

		if (++ticks > 300)
		{
			aita::lose();
			return 0;
		}
	}

	return -1;
}