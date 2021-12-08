#include <SFML/Graphics.hpp>
#include "rlottie.h"
#include "util.hpp"
#include <objbase.h>
#include "cxxopts/cxxopts.hpp"

// SFML
#pragma comment(lib, "winmm")
#pragma comment(lib, "freetype")
#pragma comment(lib, "opengl32")
#if _DEBUG
#pragma comment(lib, "sfml-system-s-d")
#pragma comment(lib, "sfml-graphics-s-d")
#pragma comment(lib, "sfml-window-s-d")
#pragma comment(lib, "sfml-main-d")
#else
#pragma comment(lib, "sfml-system-s")
#pragma comment(lib, "sfml-graphics-s")
#pragma comment(lib, "sfml-window-s")
#pragma comment(lib, "sfml-main")
#endif // _DEBUG


// rlottie
#pragma comment(lib, "rlottie")

using namespace std;

void makeWindowTransparent(sf::RenderWindow& window)
{
	HWND hwnd = window.getSystemHandle();
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
}
void makeWindowOpaque(sf::RenderWindow& window)
{
	HWND hwnd = window.getSystemHandle();
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
	RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}
inline void setWindowAlpha(sf::RenderWindow& window, sf::Uint8 alpha = 255)
{
	SetLayeredWindowAttributes(window.getSystemHandle(), 0, alpha, LWA_ALPHA);
}

int main(int argc, char** argv) {
	
	cxxopts::Options options("lottiePlayer", "lottie-player usage");

	options.add_options()
		("w,width", "width of the window", cxxopts::value<size_t>()->default_value("800"))
		("h,height", "height of the window", cxxopts::value<size_t>()->default_value("600"))
		("f,file", "lottie filename", cxxopts::value<string>())
		("t,title", "title of window", cxxopts::value<string>())
		("bgcolor", "background color (rgba)", cxxopts::value<string>()->default_value("ffffffff"))
		("help", "Print usage")
		;

	auto cmdline = options.parse(argc, argv);

	if (cmdline.count("help")) {
		cout << options.help() << endl;
		return 0;
	}

	const size_t w = cmdline["width"].as<size_t>();
	const size_t h = cmdline["height"].as<size_t>();
	string filename = cmdline["file"].as<string>();
	string title = cmdline["title"].as<string>();
	string hex = cmdline["bgcolor"].as<string>();

	if (hex.length() < 6 || hex.length() > 8) hex = "ffffffff";
	else if (hex.length() == 6) hex += "ff";

	uint32_t backColor = (uint32_t)strtoul(hex.c_str(), nullptr, 16);


	sf::ContextSettings settings;
	settings.antialiasingLevel = 16;
	sf::RenderWindow window(sf::VideoMode(w, h), title, sf::Style::None, settings);
	window.setVerticalSyncEnabled(true);

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


	sf::Color backgroundColor(backColor);
	
	auto animator = rlottie::Animation::loadFromFile(filename);
	auto frameCount = animator->totalFrame();

	const size_t bufferSize = w * h;
	auto buffer = unique_ptr<uint32_t[]>(new uint32_t[bufferSize]);

	rlottie::Surface surface(buffer.get(), w, h, w * 4);

	sf::Texture texture;
	texture.create(w, h);	
	texture.setSmooth(true);

	sf::Sprite sprite(texture);

	HWND hwnd = window.getSystemHandle();
	makeWindowTransparent(window);
		
	size_t frameNo = 0;

	sf::Time interval = sf::milliseconds(35);

	sf::Uint8 alpha = 0;

	sf::RenderStates renderstates;
	
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				window.close();
			}
			else if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Escape)
					window.close();
			}
			else if (event.type == sf::Event::MouseButtonPressed) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					grabbedOffset = window.getPosition() - sf::Mouse::getPosition();
					grabbedWindow = true;
				}
			}
			else if (event.type == sf::Event::MouseButtonReleased) {
				if (event.mouseButton.button == sf::Mouse::Left)
					grabbedWindow = false;
			}
			else if (event.type == sf::Event::MouseMoved) {
				if (grabbedWindow)
					window.setPosition(sf::Mouse::getPosition() + grabbedOffset);
			}
		}

		animator->renderSync(frameNo, surface);
		frameNo++;
		if (frameNo > frameCount) {
			frameNo = 0;
		}

		// argb -> rgba
		uint32_t* data = buffer.get();
		for (size_t idx = 0; idx < bufferSize; ++idx) {
			data[idx] = util::argbToRgba(data[idx]);
		}

		texture.update((sf::Uint8*)buffer.get());
		
		window.clear(backgroundColor);
		
		window.draw(sprite, renderstates);
		window.draw(rectangle);

		window.display();

		sf::sleep(interval);

	}

	return 0;
}