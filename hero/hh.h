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

struct GameSoundBuffer
{
	uint32_t samplesPerSecond;
	uint16_t noOfChannels;
	uint16_t bitsPerSample;
	uint32_t bufferLengthInSec;
	uint32_t soundCursor;
};

enum SoundWave
{
	SQUARE_WAVE,
	SINE_WAVE
};

static void FillColorsInBackBuffer(GameBackBuffer* buffer);
static GameSoundBuffer InitializeSoundProperties();
