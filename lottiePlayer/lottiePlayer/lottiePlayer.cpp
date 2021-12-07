#include <SFML/Graphics.hpp>
#pragma comment(lib, "winmm")
#pragma comment(lib, "freetype")
#pragma comment(lib, "opengl32")
#pragma comment(lib, "sfml-system-s")
#pragma comment(lib, "sfml-graphics-s")
#pragma comment(lib, "sfml-window-s")
#pragma comment(lib, "sfml-main")

#include "rlottie.h"
#pragma comment(lib, "rlottie")
inline uint32_t fromArgb(uint32_t argb)
{
	return
		// Source is in format: 0xAARRGGBB
		((argb & 0x00FF0000) >> 16) | //______RR
		((argb & 0x0000FF00)) | //____GG__
		((argb & 0x000000FF) << 16)  | //___BB____
		((argb & 0xFF000000));         //AA______
		// Return value is in format:  0xAABBGGRR 
}

int main()
{
	size_t w = 845;
	size_t h = 468;

	sf::ContextSettings settings;
	settings.antialiasingLevel = 2;
	sf::RenderWindow window(sf::VideoMode(w, h), "", sf::Style::None, settings);
	sf::CircleShape shape(100.f);
	shape.setFillColor(sf::Color::Green);
	float thickness = 1.0;
	sf::Vector2f v2((float)w-thickness*2, (float)h-thickness*2);
	sf::RectangleShape rectangle(v2);
	rectangle.setOutlineColor(sf::Color(59, 59, 59));
	rectangle.setOutlineThickness(2.0);
	rectangle.setFillColor(sf::Color(0, 0, 0, 0));
	rectangle.setOrigin(sf::Vector2f(-thickness, -thickness));

	sf::Vector2i grabbedOffset;
	bool grabbedWindow = false;

	std::string filename = "test.json";
	auto animator = rlottie::Animation::loadFromFile(filename);
	auto frameCount = animator->totalFrame();

	auto buffer = std::unique_ptr<uint32_t[]>(new uint32_t[w * h]);

	rlottie::Surface surface(buffer.get(), w, h, w * 4);

	sf::Texture texture;
	texture.create(w, h);

	sf::Sprite sprite(texture);

	
	size_t frameNo = 0;

	sf::Time interval = sf::milliseconds(20);
	
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::KeyPressed)
			{
				if (event.key.code == sf::Keyboard::Escape)
					window.close();
			}
			else if (event.type == sf::Event::MouseButtonPressed)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
				{
					grabbedOffset = window.getPosition() - sf::Mouse::getPosition();
					grabbedWindow = true;
				}
			}
			else if (event.type == sf::Event::MouseButtonReleased)
			{
				if (event.mouseButton.button == sf::Mouse::Left)
					grabbedWindow = false;
			}
			else if (event.type == sf::Event::MouseMoved)
			{
				if (grabbedWindow)
					window.setPosition(sf::Mouse::getPosition() + grabbedOffset);
			}
		}

		animator->renderSync(frameNo++, surface);

		if (frameNo > frameCount) {
			frameNo = 0;
		}

		// argb -> rgba
		uint32_t* data = buffer.get();
		for (size_t idx = 0; idx < w*h; ++idx) {
			data[idx] = fromArgb(data[idx]);
		}


		texture.update((sf::Uint8*)buffer.get());

		window.clear(sf::Color(255, 255, 255, 0));
		
		
		window.draw(sprite);
		window.draw(rectangle);

		window.display();

		sf::sleep(interval);
	}

	return 0;
}

void BlurBehindWindow() {
	/*
	{
#include <dwmapi.h>
#pragma comment(lib, "dwmapi")
		DWM_BLURBEHIND bb = { 0 };
		bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
		bb.fEnable = true;
		bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
		DwmEnableBlurBehindWindow(window.getSystemHandle(), &bb);
	}
	*/
}