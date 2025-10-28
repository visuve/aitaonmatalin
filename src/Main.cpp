namespace aita
{
	constexpr uint32_t WindowWidth = 800;
	constexpr uint32_t WindowHeight = 600;

	constexpr float FenceWidth = 20.0f;
	constexpr float FenceHeight = 200.0f;
	constexpr float FenceHalf = FenceWidth / 2.0f;
	constexpr float FenceX = WindowWidth / 2 - FenceHalf;
	constexpr float FenceY = WindowHeight - FenceHeight;
	constexpr float FenceLeft = FenceX;
	constexpr float FenceMiddle = FenceLeft + FenceHalf;
	constexpr float FenceRight = FenceLeft + FenceWidth;

	float Gravity = 0.5f;
	constexpr float JumpVelocity = -16.0f;
	constexpr float HitPenaltyHorizontal = 15.0f;
	constexpr float HitPenaltyVertical = 10.0f;

	class Player : public sf::Drawable
	{
	public:
		static constexpr float Radius = 40.0f;
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

	auto c = [](uint16_t duration) { _beep(261, duration); };
	auto cis = [](uint16_t duration) { _beep(278, duration); };
	auto d = [](uint16_t duration) { _beep(294, duration); };
	auto dis = [](uint16_t duration) { _beep(311, duration); };
	auto e = [](uint16_t duration) { _beep(330, duration); };
	auto f = [](uint16_t duration) { _beep(349, duration); };
	auto fis = [](uint16_t duration) { _beep(370, duration); };
	auto g = [](uint16_t duration) { _beep(392, duration); };
	auto gis = [](uint16_t duration) { _beep(415, duration); };
	auto a = [](uint16_t duration) { _beep(440, duration); };
	auto ais = [](uint16_t duration) { _beep(466, duration); };
	auto h = [](uint16_t duration) { _beep(494, duration); };
	auto a2 = [](uint16_t duration) { _beep(220, duration); };
	auto h2 = [](uint16_t duration) { _beep(247, duration); };
	auto p = [](uint16_t duration) { _beep(0, duration); };

	void lose()
	{
		p(100);
		h(200);
		ais(200);
		a(200);
		gis(200);
		g(200);
		fis(200);
		f(200);
		e(200);
		dis(200);
		d(200);
		cis(200);
		c(500);
		_sleep(500);
	}

	void win()
	{
		p(100);
		e(200); p(100);
		dis(200); p(100);
		e(200); p(100);
		dis(200); p(100);
		e(200); p(100);
		h2(200); p(100);
		d(200); p(100);
		c(200); p(100);
		a2(500); p(100);
		_sleep(1000);
	}
}

int main(int argc, char** argv)
{
	aita::Player player;

	if (argc > 1 && std::string(argv[1]) == "--no-gravity")
	{
		aita::Gravity = 0.0f;
	}

	sf::RenderWindow window(sf::VideoMode({ aita::WindowWidth, aita::WindowHeight }, 8), "Aita on matalin");

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
			player.move({ 4.0f, 0.0f });
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
		{
			player.move({ -4.0f, 0.0f });
		}

		if (aita::Gravity == 0.0f)
		{
			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
			{
				player.move({ 0.0f, -2.0f });
			}

			if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
			{
				player.move({ 0.0f, 2.0f });
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