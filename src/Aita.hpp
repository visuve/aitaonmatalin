#pragma once

namespace aita
{
	struct Configuration
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

		Configuration(float width, float height);
	};

	class Player : public sf::Drawable
	{
	public:
		const float Radius;
		const float Diameter;
		const float MinimumX;
		const float MaximumX;
		const float MinimumY;
		const float MaximumY;

		Player(const Configuration& config);

		void reset();
		void jump();
		void move(sf::Vector2f direction);
		void update();
		sf::Vector2f bottomRight() const;
		void draw(sf::RenderTarget& target, sf::RenderStates states) const override;

	private:
		const aita::Configuration& _config;
		sf::Vector2f _position;
		sf::Vector2f _velocity;
		sf::CircleShape _shape;
	};

	class Game
	{
	public:
		Configuration Config;
		Game(float width, float height);
		operator bool() const;
		int32_t play();

	private:
		void onClose(const sf::Event::Closed& closed);
		void onKeyPressed(const sf::Event::KeyPressed& keyPressed);
		void onMove();

		Player _player;
		sf::RenderWindow _window;
		sf::RectangleShape _fence;
	};
}