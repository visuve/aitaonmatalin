namespace aita
{
	constexpr uint32_t WindowWidth = 800;
	constexpr uint32_t WindowHeight = 600;

	constexpr float FenceWidth = 20.0f;
	constexpr float FenceHeight = 200.0f;
	constexpr float FenceX = WindowWidth / 2 - FenceWidth / 2;
	constexpr float FenceY = WindowHeight - FenceHeight;

	constexpr float Gravity = 0.5f;

	class Player : public sf::Drawable
	{
	public:
		static constexpr float Radius = 40.0f;
		static constexpr float Diameter = Radius * 2.0f;
		static constexpr float MinimumX = 0.0f;
		static constexpr float MaximumX = WindowWidth - Diameter;
		static constexpr float MinimumY = 0.0f;
		static constexpr float MaximumY = WindowHeight - Diameter;
		static constexpr float JumpVelocity = -16.0f;

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

			// Fence collision
			if (_position.y > FenceY - Diameter)
			{
				if (_position.x > FenceX + FenceWidth)
				{
					// Right hit
					_shape.setFillColor(sf::Color::Cyan);
				}
				else if (_position.x > FenceX - Diameter)
				{
					// Left hit
					_shape.setFillColor(sf::Color::Blue);
				}
				else
				{
					// Below fence, no hit
					_shape.setFillColor(sf::Color::Green);
				}
			}
			else
			{
				// Higher than fence
				_shape.setFillColor(sf::Color::Green);
			}
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
}

int main()
{
	aita::Player player;

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
	fence.setPosition({ aita::FenceX, aita::FenceY});

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

		// TODO: add a no gravity mode where up and down keys move the player freely

		/*if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
		{
			player.move({ 0.0f, -2.0f });
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
		{
			player.move({ 0.0f, 2.0f });
		}*/

		player.update();

		window.clear();
		window.draw(fence);
		window.draw(player);

		window.display();
	}
}