#pragma once
#include <windows.h>

class w32window {
public:
	static void makeWindowTransparent(HWND hwnd) {		
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	}

	static void makeWindowToolWindow(HWND hwnd) {		
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
	}

	static void makeWindowOpaque(HWND hwnd) {		
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		RedrawWindow(hwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
	}

	static void setWindowAlpha(HWND hwnd, BYTE alpha = 255) {
		SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
	}

};
