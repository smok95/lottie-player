#pragma once
#include <stdint.h>

class util {
public:
	static inline uint32_t argbToRgba(uint32_t argb)
	{
		return
			// Source is in format: 0xAARRGGBB
			((argb & 0x00FF0000) >> 16) |	//______RR
			((argb & 0x0000FF00)) |			//____GG__
			((argb & 0x000000FF) << 16) |	//__BB____
			((argb & 0xFF000000));			//AA______
			// Return value is in format:  0xAABBGGRR 
	}
};

