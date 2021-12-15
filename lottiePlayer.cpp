#include <SFML/Graphics.hpp>
#include <windows.h>
#include "rlottie.h"
#include "util.hpp"
#include <objbase.h>
#include "cxxopts/cxxopts.hpp"
#include "w32window.hpp"
#include <tchar.h>

// SFML
#pragma comment(lib, "winmm")
#pragma comment(lib, "freetype")
#pragma comment(lib, "opengl32")
#if _DEBUG
#pragma comment(lib, "sfml-system-s-d")
#pragma comment(lib, "sfml-graphics-s-d")
#pragma comment(lib, "sfml-window-s-d")
#else
#pragma comment(lib, "sfml-system-s")
#pragma comment(lib, "sfml-graphics-s")
#pragma comment(lib, "sfml-window-s")
#endif // _DEBUG


// rlottie
#pragma comment(lib, "rlottie")

using namespace std;

HINSTANCE gInst = nullptr;

LPCTSTR CLASS_NAME = _T("SFML App");
#define TIMERID_FADEIN 100
#define TIMERID_FADEOUT 101

int gFadeInValue = 0;
int gFadeOutValue = 0xff;
bool gFadeout = false;
bool gFadeIn = false;

int main(int argc, char** argv);
LRESULT CALLBACK onEvent(HWND handle, UINT message, WPARAM wp, LPARAM lp);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_
	LPSTR lpCmdLine, _In_ int nShowCmd) {
	gInst = hInstance;
	return main(__argc, __argv);
}

int main(int argc, char** argv) {
	
	cxxopts::Options options("lottiePlayer", "lottie-player usage");
	options.allow_unrecognised_options();

	options.add_options()
		("w,width", "width of the window", cxxopts::value<size_t>()->default_value("800"))
		("h,height", "height of the window", cxxopts::value<size_t>()->default_value("600"))
		("f,file", "lottie filename", cxxopts::value<string>()->default_value(""))
		("t,title", "title of window", cxxopts::value<string>()->default_value(""))
		("bgcolor", "background color (rgba)", cxxopts::value<string>()->default_value("ffffffff"))
		("s,speed", "speed", cxxopts::value<double>()->default_value("1.0"))
		("a,alpha", "alpha (0.0 ~ 1.0)", cxxopts::value<double>()->default_value("1.0"))
		("hide", "window hide", cxxopts::value<bool>()->default_value("false"))
		("timeout", "app close timeout(msec)", cxxopts::value<size_t>()->default_value("0"))
		("fadein", "fade-in", cxxopts::value<bool>()->default_value("false"))
		("fadeout", "fade-out", cxxopts::value<bool>()->default_value("false"))
		("help", "Print usage")
		;

	auto cmdline = options.parse(argc, argv);
	
	if (cmdline.count("help")) {		
		MessageBox(nullptr, options.help().c_str(), "lottie-player", MB_OK | MB_SYSTEMMODAL | MB_TOPMOST);
		
		return 0;
	}

	const size_t w = cmdline["width"].as<size_t>();
	const size_t h = cmdline["height"].as<size_t>();
	string filename = cmdline["file"].as<string>();
	string title = cmdline["title"].as<string>();
	string hex = cmdline["bgcolor"].as<string>();
	double speed = cmdline["speed"].as<double>();
	double alpha = cmdline["alpha"].as<double>();
	const bool visible = cmdline["hide"].as<bool>()==false;
	size_t timeout = cmdline["timeout"].as<size_t>();
	const bool fadein = cmdline["fadein"].as<bool>();
	const bool fadeout = cmdline["fadeout"].as<bool>();

	gFadeout = fadeout;
	gFadeIn = fadein;

	if (hex.length() < 6 || hex.length() > 8) hex = "ffffffff";
	else if (hex.length() == 6) hex += "ff";

	uint32_t backColor = (uint32_t)strtoul(hex.c_str(), nullptr, 16);

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

	double framerate = animator->frameRate() * speed;


	const size_t bufferSize = w * h;
	auto buffer = unique_ptr<uint32_t[]>(new uint32_t[bufferSize]);

	rlottie::Surface surface(buffer.get(), w, h, w * 4);

	sf::Texture texture;
	texture.create(w, h);	
	texture.setSmooth(true);

	sf::Sprite sprite(texture);
	
	size_t frameNo = 0;

	sf::Time interval = sf::microseconds(1000000.0/framerate);

	sf::RenderStates renderstates;
	
	// Define a class for our main window
	WNDCLASS windowClass;
	windowClass.style = 0;
	windowClass.lpfnWndProc = &onEvent;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = gInst;
	windowClass.hIcon = nullptr;
	windowClass.hCursor = nullptr;
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = CLASS_NAME;
	RegisterClass(&windowClass);

	POINT pos = { 0, };
	{
		// Compute position and size
		HDC screenDC = GetDC(nullptr);
		pos.x = (GetDeviceCaps(screenDC, HORZRES) - static_cast<int>(w)) / 2;
		pos.y = (GetDeviceCaps(screenDC, VERTRES) - static_cast<int>(h)) / 2;
		ReleaseDC(nullptr, screenDC);
	}

	DWORD style = WS_POPUP;
	if (visible) {
		style |= WS_VISIBLE;
	}
	
	DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED| WS_EX_TOPMOST;

	//w32window::makeWindowTransparent(hwnd);
	//w32window::makeWindowToolWindow(hwnd);
	
	// Let's create the main window
	HWND hMainWnd = CreateWindowEx(exStyle, CLASS_NAME, title.c_str(), style, 
		pos.x, pos.y, w, h, nullptr, nullptr, gInst, nullptr);


	if (fadein) {
		if (visible) {
			w32window::setWindowAlpha(hMainWnd, 0);
			SetTimer(hMainWnd, TIMERID_FADEIN, 20, nullptr);
		}
	}
	else {
		w32window::setWindowAlpha(hMainWnd, (BYTE)(255.0 * alpha));
	}

	// Let's create two SFML views
	const DWORD childStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
	HWND hView = CreateWindow(TEXT("STATIC"), nullptr, childStyle, 0, 0, w, h, 
		hMainWnd, nullptr, gInst, nullptr);
	sf::RenderWindow lottieWnd(hView, sf::ContextSettings(32, 0, 8));
	
	// Loop until a WM_QUIT message is received
	MSG msg;
	msg.message = static_cast<UINT>(~WM_QUIT);

	DWORD start = GetTickCount();

	/*
	sf::Text text;
	sf::Font font;
	if (!font.loadFromFile("C:/Windows/Fonts/malgun.ttf"))
	{
		// error...
	}

	// select the font
	text.setFont(font); // font is a sf::Font

	// set the string to display
	text.setString(L"");

	// set the character size
	text.setCharacterSize(14); // in pixels, not points!

	// set the color
	text.setFillColor(sf::Color(99,99,99));

	// set the text style
	//text.setStyle(sf::Text::Bold);

	sf::FloatRect bounds = text.getLocalBounds();
	text.setPosition(20, 20);
	//text.setPosition((w-bounds.width)/2, h - (bounds.height + 30) );
	*/
	while (msg.message != WM_QUIT) {

		if (timeout > 0) {
			if (timeout <= (GetTickCount() - start)) {
				if (gFadeout) {
					SetTimer(hMainWnd, TIMERID_FADEOUT, 20, nullptr);
					timeout = 0;
				}
				else {
					break;
				}
			}
		}

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			// If a message was waiting in the message queue, process it
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
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

			lottieWnd.clear(backgroundColor);

			lottieWnd.draw(sprite, renderstates);
			//lottieWnd.draw(text);
			lottieWnd.draw(rectangle);
			

			lottieWnd.display();


			sf::sleep(interval);
		}
	}

	// Close our SFML views before destroying the underlying window
	lottieWnd.close();

	// Destroy the main window (all its child controls will be destroyed)
	DestroyWindow(hMainWnd);
	
	// Don't forget to unregister the window class
	UnregisterClass(CLASS_NAME, gInst);

	return EXIT_SUCCESS;
}

LRESULT CALLBACK onEvent(HWND handle, UINT message, WPARAM wp, LPARAM lp) {
	switch (message)
	{
		// Quit when we close the main window
	case WM_CLOSE:
	{
		if (gFadeout) {
			SetTimer(handle, TIMERID_FADEOUT, 20, nullptr);
		}
		else {
			PostQuitMessage(0);
		}
		return 0;
	}
	case WM_KEYDOWN: 
	{
		if (wp == VK_ESCAPE) {
			if (gFadeout) {
				SetTimer(handle, TIMERID_FADEOUT, 20, nullptr);
			}
			else {
				PostQuitMessage(0);
			}
			return 0;
		}
	}break;
	case WM_TIMER:
	{
		if (wp == TIMERID_FADEIN) {
			gFadeInValue += 12;
			if (gFadeInValue > 0xff) {
				gFadeInValue = 0xff;
			}
			w32window::setWindowAlpha(handle, gFadeInValue);

			if (gFadeInValue >= 0xff) {
				KillTimer(handle, wp);
				gFadeInValue = 0;
			}
		}
		else if (wp == TIMERID_FADEOUT) {
			gFadeOutValue -= 12;
			if (gFadeOutValue < 0) {
				gFadeOutValue = 0;
			}
			w32window::setWindowAlpha(handle, gFadeOutValue);

			if (gFadeOutValue <= 0) {
				gFadeOutValue = 0xff;
				KillTimer(handle, wp);
				PostQuitMessage(0);
			}
		}
	}break;
	case WM_SHOWWINDOW:
	{
		if (wp == TRUE) {
			if (gFadeIn) {
				w32window::setWindowAlpha(handle, 0);
				SetTimer(handle, TIMERID_FADEIN, 20, nullptr);
			}
		}
	}break;
	case WM_LBUTTONDOWN:
	{

	}break;
	/*
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

		*/

	}
	
	return DefWindowProc(handle, message, wp, lp);
}