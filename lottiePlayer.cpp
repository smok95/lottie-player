#include <SFML/Graphics.hpp>
#include <windows.h>
#include "rlottie.h"
#include "util.hpp"
#include <objbase.h>
#include "cxxopts/cxxopts.hpp"
#include "w32window.hpp"
#include <tchar.h>
#include <filesystem>
#include <ciso646>

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

class Settings {
public:
	size_t width;
	size_t height;
	string filename;
	string title;
	double speed;
	double alpha;		// 0.0 ~ 1.0
	bool visible;
	int fadeInValue;	// 0 ~ 0xff
	int fadeOutValue;	// 0xff ~ 0
	bool useFadeIn;		// 프로그램 시작시 fade-in 효과 사용
	bool useFadeOut;	// 프로그램 종료시 fade-out 효과 사용
	size_t timeout;		// msec
	sf::Color backgroundColor;
	bool redraw;
	Settings():fadeInValue(0), fadeOutValue(0xff), useFadeIn(false), width(0),
		height(0), useFadeOut(false), speed(0.0), alpha(1.0), visible(true), 
		timeout(0), redraw(false) {}

	bool initWithCommandline(cxxopts::ParseResult& cmdline);

	bool isValid() const;
};

Settings settings_;

class Win {
public:
	Win(HINSTANCE hInst, LPCSTR className);
	virtual ~Win();

private:
	std::string className_;
	HINSTANCE hInstance_;
};


int main(int argc, char** argv);
LRESULT CALLBACK onEvent(HWND handle, UINT message, WPARAM wp, LPARAM lp);
bool isSupportedImageExtension(const std::filesystem::path& extension);

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

	settings_.initWithCommandline(cmdline);

	if (not settings_.isValid()) return 0;

	float thickness = 1.0;
	sf::Vector2f v2((float)settings_.width-thickness*2, 
		(float)settings_.height-thickness*2);

	sf::RectangleShape rectangle(v2);
	rectangle.setOutlineColor(sf::Color(59, 59, 59));
	rectangle.setOutlineThickness(2.0);
	rectangle.setFillColor(sf::Color(0, 0, 0, 0));
	rectangle.setOrigin(sf::Vector2f(-thickness, -thickness));


	filesystem::path p = settings_.filename;	
	if (not p.has_extension()) return 0;

	const size_t w = settings_.width;
	const size_t h = settings_.height;

	unique_ptr<rlottie::Animation> animator;
	size_t frameCount = 0;
	double frameRate = 0.0;
	sf::Time interval = sf::Time::Zero;
	size_t bufferSize = 0;
	unique_ptr<uint32_t[]> buffer;

	sf::Texture texture;
	texture.setSmooth(true);

	const bool isImageFile = isSupportedImageExtension(p.extension());

	rlottie::Surface surface;
	if (isImageFile) {
		// 이미지 표시
		frameCount = 1;
		texture.loadFromFile(settings_.filename);
	}
	else {
		// lottie 표시
		animator = move(rlottie::Animation::loadFromFile(settings_.filename));
		frameCount = animator->totalFrame();
		frameRate = animator->frameRate() * settings_.speed;
		interval = sf::microseconds((sf::Int64)(1000000.0 / frameRate));

		bufferSize = w * h;
		buffer = move(unique_ptr<uint32_t[]>(new uint32_t[bufferSize]));

		surface = rlottie::Surface(buffer.get(), w, h, w * 4);
		texture.create(w, h);
	}

	if (frameCount == 0) return 0;


	sf::Sprite sprite(texture);
	auto textureSize = texture.getSize();
	sprite.setScale((float)w/textureSize.x, (float)h / textureSize.y);
	
	size_t frameNo = 0;

	Win mainWin(gInst, CLASS_NAME);
	
	POINT pos = { 0, };
	{
		// Compute position and size
		HDC screenDC = GetDC(nullptr);
		pos.x = (GetDeviceCaps(screenDC, HORZRES) - static_cast<int>(w)) / 2;
		pos.y = (GetDeviceCaps(screenDC, VERTRES) - static_cast<int>(h)) / 2;
		ReleaseDC(nullptr, screenDC);
	}

	DWORD style = WS_POPUP;
	if (settings_.visible) {
		style |= WS_VISIBLE;
	}
	
	DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_LAYERED| WS_EX_TOPMOST;

	// Let's create the main window
	HWND hMainWnd = CreateWindowEx(exStyle, CLASS_NAME, settings_.title.c_str(), style, 
		pos.x, pos.y, w, h, nullptr, nullptr, gInst, nullptr);


	if (settings_.useFadeIn) {
		if (settings_.visible) {
			w32window::setWindowAlpha(hMainWnd, 0);
			SetTimer(hMainWnd, TIMERID_FADEIN, 20, nullptr);
		}
	}
	else {
		w32window::setWindowAlpha(hMainWnd, (BYTE)(255.0 * settings_.alpha));
	}

	// Let's create two SFML views
	const DWORD childStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS;
	HWND hView = CreateWindow(TEXT("STATIC"), nullptr, childStyle, 0, 0, w, h, 
		hMainWnd, nullptr, gInst, nullptr);
	sf::RenderWindow renderWin(hView, sf::ContextSettings(0, 0, 8));
	
	// Loop until a WM_QUIT message is received
	MSG msg;
	msg.message = static_cast<UINT>(~WM_QUIT);

	ULONGLONG start = GetTickCount64();

	while (msg.message != WM_QUIT) {

		if (settings_.timeout > 0) {
			if (settings_.timeout  <= (GetTickCount64() - start)) {
				if (settings_.useFadeOut) {
					SetTimer(hMainWnd, TIMERID_FADEOUT, 20, nullptr);
					settings_.timeout = 0;
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

			if (animator) {
				// lottie인 경우
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

				renderWin.clear(settings_.backgroundColor);

				renderWin.draw(sprite);
				renderWin.draw(rectangle);

				renderWin.display();

				sf::sleep(interval);
			}
			else {
				// image인 경우
				if (frameNo < frameCount || settings_.redraw) {
					frameNo++;
					renderWin.clear(settings_.backgroundColor);

					renderWin.draw(sprite);
					//renderWin.draw(rectangle);

					renderWin.display();

					settings_.redraw = false;
				}
				sf::sleep(sf::milliseconds(10));
			}
			
		}
	}

	// Close our SFML views before destroying the underlying window
	renderWin.close();

	// Destroy the main window (all its child controls will be destroyed)
	DestroyWindow(hMainWnd);
	
	return EXIT_SUCCESS;
}

bool Settings::initWithCommandline(cxxopts::ParseResult& cmdline) {
	width = cmdline["width"].as<size_t>();
	height = cmdline["height"].as<size_t>();
	filename = cmdline["file"].as<string>();
	title = cmdline["title"].as<string>();
	string hex = cmdline["bgcolor"].as<string>();
	speed = cmdline["speed"].as<double>();
	alpha = cmdline["alpha"].as<double>();
	visible = cmdline["hide"].as<bool>() == false;
	timeout = cmdline["timeout"].as<size_t>();
	useFadeIn = cmdline["fadein"].as<bool>();
	useFadeOut = cmdline["fadeout"].as<bool>();

	if (hex.length() < 6 || hex.length() > 8) hex = "ffffffff";
	else if (hex.length() == 6) hex += "ff";

	backgroundColor = sf::Color((uint32_t)strtoul(hex.c_str(), nullptr, 16));

	return true;
}

bool Settings::isValid() const {
	if (filename.empty()) return false;

	return true;
}

LRESULT CALLBACK onEvent(HWND handle, UINT message, WPARAM wp, LPARAM lp) {
	switch (message)
	{
		// Quit when we close the main window
	case WM_CLOSE:
	{
		if (settings_.useFadeOut) {
			SetTimer(handle, TIMERID_FADEOUT, 20, nullptr);
		}
		else {
			PostQuitMessage(0);
		}
		return 0;
	}break;
	case WM_KEYDOWN: 
	{
		if (wp == VK_ESCAPE) {
			if (settings_.useFadeOut) {
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
			settings_.fadeInValue += 12;
			if (settings_.fadeInValue > 0xff) {
				settings_.fadeInValue = 0xff;
			}
			w32window::setWindowAlpha(handle, settings_.fadeInValue);

			if (settings_.fadeInValue >= 0xff) {
				KillTimer(handle, wp);
				settings_.fadeInValue = 0;
			}
		}
		else if (wp == TIMERID_FADEOUT) {
			settings_.fadeOutValue -= 12;
			if (settings_.fadeOutValue < 0) {
				settings_.fadeOutValue = 0;
			}
			w32window::setWindowAlpha(handle, settings_.fadeOutValue);

			if (settings_.fadeOutValue <= 0) {
				settings_.fadeOutValue = 0xff;
				KillTimer(handle, wp);
				PostQuitMessage(0);
			}
		}
	}break;
	case WM_SHOWWINDOW:
	{
		if (wp == TRUE) {
			if (settings_.useFadeIn) {
				settings_.redraw = true;
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

bool isSupportedImageExtension(const std::filesystem::path& extension) {
	// https://www.sfml-dev.org/documentation/2.5.1/classsf_1_1Image.php
	// The supported image formats are bmp, png, tga, jpg, gif, psd, hdr and pic.
	const wstring imgFormats[] = {
		L".bmp", L".png", L".tga", L".jpg", L".gif", L".psd", L".hdr", L".pic" };

	for (auto& ext : imgFormats) {
		if (_wcsicmp(ext.c_str(), extension.c_str()) == 0) {
			return true;
			break;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
Win::Win(HINSTANCE hInst, LPCSTR className):hInstance_(hInst) {
	className_ = className ? className : "";


	// Define a class for our main window
	WNDCLASS windowClass;
	windowClass.style = 0;
	windowClass.lpfnWndProc = &onEvent;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance_;
	windowClass.hIcon = nullptr;
	windowClass.hCursor = nullptr;
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = className_.c_str();
	RegisterClass(&windowClass);
}

Win::~Win() {
	UnregisterClass(className_.c_str(), hInstance_);
}

/*
	SFML Text example.

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
