int main()
{
	sf::RenderWindow window(sf::VideoMode({ 800, 600 }, 8), "Aita on matalin");

	window.setVerticalSyncEnabled(true);
	window.setFramerateLimit(30);

	const auto onClose = [&window](const sf::Event::Closed&)
	{
		window.close();
	};

	const auto onKeyPressed = [&window](const sf::Event::KeyPressed& keyPressed)
	{
		if (keyPressed.scancode == sf::Keyboard::Scancode::Escape)
		{
			window.close();
		}
	};

	sf::RectangleShape fence({ 20, 200 });
	fence.setFillColor(sf::Color::Red);
	fence.setPosition({ 400, 400 });

	sf::CircleShape jumper(40, 8);
	jumper.setFillColor(sf::Color::Green);
	jumper.setPosition({ 50, 520 });

	while (window.isOpen())
	{
		window.handleEvents(onClose, onKeyPressed);
		window.clear();
		window.draw(fence);
		window.draw(jumper);
		window.display();
	}
}