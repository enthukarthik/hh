#include <stdint.h>
#include <stdbool.h>
#include "hh.h"

static struct GameAnimationState g_animationState;

static void
SetAnimationState()
{
	g_animationState.animate = true;
	for(int i = 0; i < 3; ++i) {
		g_animationState.colorOffset[i] = 0;
		g_animationState.incrementValue[i] = 0;
	}
}

static void
ToggleAnimation()
{
	g_animationState.animate = !g_animationState.animate;
}

static void
ChangeAnimationIncrementValues(int32_t increment)
{
	for(int i = 0; i < 3; ++i) {
		g_animationState.incrementValue[i] += increment;
	}
}

static void
SetAnimationIncrementValue(uint32_t channelIndex, int32_t increment)
{
	if(channelIndex < 3) {
		g_animationState.incrementValue[channelIndex] = increment;
	}
}

// 32 bit color is expected to be in ARGB format. In the memory it'll be written as BGRA due to little endian architecture
#define MEMRGB(r, g, b) (((uint32_t)(r)) << 16 | ((uint32_t)(g)) << 8 | (uint32_t)(b))

static void
FillColorsInBackBuffer(
	GameBackBuffer* buffer
)
{
	uint32_t* mem = (uint32_t*) buffer->memory;

	for(uint32_t row = 0; row < buffer->height; ++row) {
		for(uint32_t col = 0; col < buffer->width; ++col) {
			uint32_t* pixel = mem + row * buffer->width + col;
			uint8_t red   = (uint8_t) (row + g_animationState.colorOffset[0]);
			uint8_t green = (uint8_t) (col + g_animationState.colorOffset[1]);
			uint8_t blue  = (uint8_t) (row * col + g_animationState.colorOffset[2]);
			*pixel = MEMRGB(red, green, blue);
		}
	}

	if(g_animationState.animate) {
		for(int i = 0; i < 3; ++i) {
			g_animationState.colorOffset[i] += g_animationState.incrementValue[i];
		}
	}
}
