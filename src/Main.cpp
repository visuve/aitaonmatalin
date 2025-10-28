class Player : public sf::Drawable
{
public:
	Player() :
		_shape(40, 8),
		_position(20.0f, 520.0f)
	{
		_shape.setPosition(_position);
		_shape.setFillColor(sf::Color::Green);
	}

	~Player() = default;

	void jump()
	{
		if (!_jumping)
		{
			_velocity.y = -16.0f;
			_jumping = true;
		}
	}

	void move(float dx)
	{
		_position.x += dx;
	}

	void update()
	{
		_position += _velocity;

		if (_position.y >= 520.0f)
		{
			_position.y = 520.0f;
			_velocity.y = 0.0f;
			_jumping = false;
		}
		else
		{
			_velocity.y += 0.5f;
			_jumping = true;
		}

		_shape.setPosition(_position);
	}

private:
	void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		target.draw(_shape, states);
	}

	bool _jumping = false;
	sf::Vector2f _velocity;
	sf::Vector2f _position;
	sf::CircleShape _shape;
};

int main()
{
	Player player;

	sf::RenderWindow window(sf::VideoMode({ 800, 600 }, 8), "Aita on matalin");

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

		if (keyPressed.scancode == sf::Keyboard::Scancode::Up)
		{
			player.jump();
		}
	};

	sf::RectangleShape fence({ 20, 200 });
	fence.setFillColor(sf::Color::Red);
	fence.setPosition({ 400, 400 });

	while (window.isOpen())
	{
		window.handleEvents(onClose, onKeyPressed);


		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
		{
			player.move(4.0f);
		}

		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
		{
			player.move(-4.0f);
		}

		player.update();


		window.clear();
		window.draw(fence);
		window.draw(player);
		window.display();
	}
}