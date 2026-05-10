#pragma once

struct GameBackBuffer
{
	uint32_t width;
	uint32_t height;
	uint8_t bytesPerPixel;
	void* memory;
};

struct GameAnimationState
{
	bool	 animate;
	uint32_t colorOffset[3];
	uint32_t incrementValue[3];
};

static void FillColorsInBackBuffer(GameBackBuffer* buffer);
