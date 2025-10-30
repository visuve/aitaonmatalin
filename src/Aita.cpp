#include "Aita.hpp"

namespace aita
{
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

	Configuration::Configuration(float width, float height) :
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

	Player::Player(const Configuration& config) :
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

	void Player::reset()
	{
		_position = { MinimumX, MaximumY };
		_velocity = { 0.0f, 0.0f };
		_shape.setPosition(_position);
	}

	void Player::jump()
	{
		// Is on ground level
		if (_position.y >= MaximumY)
		{
			_velocity.y = _config.JumpVelocity;
		}
	}

	void Player::move(sf::Vector2f direction)
	{
		_position += direction;
	}

	void Player::update()
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

	sf::Vector2f Player::bottomRight() const
	{
		return { _position.x + Diameter, _position.y + Diameter };
	}

	void Player::draw(sf::RenderTarget& target, sf::RenderStates states) const
	{
		target.draw(_shape, states);
	}

	Game::Game(float width, float height) :
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

	Game::operator bool() const
	{
		return _window.isOpen();
	}

	int32_t Game::play()
	{
		const auto handleClose = std::bind(&Game::onClose, this, std::placeholders::_1);
		const auto handleKeypress = std::bind(&Game::onKeyPressed, this, std::placeholders::_1);

		_player.reset();

		// The score is inversely proportional to the time taken to finish the game
		// Higher score equals less time taken to make the jump
		int32_t score = Configuration::FramesPerSecond * 10;

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

	void Game::onClose(const sf::Event::Closed& closed)
	{
		_window.close();
	}

	void Game::onKeyPressed(const sf::Event::KeyPressed& keyPressed)
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

	void Game::onMove()
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
}