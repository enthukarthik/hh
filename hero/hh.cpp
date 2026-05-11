#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "hh.h"

static struct GameAnimationState g_animationState;

static void
SetAnimationState()
{
	g_animationState.animate = false;
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

static GameSoundBuffer 
InitializeSoundProperties()
{
	GameSoundBuffer properties = {};
	properties.samplesPerSecond  = 48000;	// 48 kHz. 48000 samples/sec
	properties.noOfChannels      = 2;		// Stereo channels. Left & Right
	properties.bufferLengthInSec = 2;		// 2 second buffer
	properties.bitsPerSample     = 16;		// 16 bits per channel. CD quality

	return properties;
}

#define PI 3.1415926f

static int16_t
GetSoundSample(
	GameSoundBuffer* buffer,
	enum SoundWave waveType
)
{
	uint32_t currSampleIndex = buffer->soundCursor;
	uint32_t toneFrequency = 256;						// Middle C is 261.62. Approximating to 256 cycles/sec
	int16_t maxAmplitude = 3000;						// Amplitude of the signal. -32768 to 32767
	uint32_t waveLength = buffer->samplesPerSecond / toneFrequency;
	uint32_t halfWaveLength = waveLength / 2;

	int16_t sampleVal = 0;

	switch(waveType) {
		case SQUARE_WAVE:
		{
			// Fill half the sample with sampleMax and fill the other half with -sampleMax to simulate square wave
			sampleVal = (currSampleIndex / halfWaveLength) % 2 ? maxAmplitude : -maxAmplitude;
		}
		break;

		case SINE_WAVE:
		{
			// Sine wave varies from -1 to 1 in the period 0 to 2 * PI. So split one wavelength into 2 * PI and take sample index worth of values from the cut
			// (1/(WaveLength / 2 * PI)) * currSampleIndex => currSampleIndex * 2 * PI / WaveLength
			float x = 2.0f * PI * ((float) currSampleIndex / (float) waveLength); // calculate sine period
			float sine = sinf(x);
			sampleVal = (int16_t) (sine * maxAmplitude);
		}
		break;
	}

	return sampleVal;
}

static void
FillSoundBuffer(
	enum SoundWave waveType,
	void* memory,
	unsigned long memorySize,
	GameSoundBuffer* properties
)
{
	uint32_t totalSamplesPerChannel = properties->samplesPerSecond * properties->bufferLengthInSec;
	uint32_t totalSamples = totalSamplesPerChannel * properties->noOfChannels;

	uint32_t bytesPerSample = properties->bitsPerSample / 8;
	uint32_t bytesPerSoundFrame = bytesPerSample * properties->noOfChannels;
	// Write into the sound buffer
	uint16_t* bufferRegion = (uint16_t*) memory;
	uint32_t noOfSamplesPerRegion = memorySize / bytesPerSoundFrame; // bytes by bytes/sample => sample. Each iteration we're writing two samples for each channel
	for(uint32_t i = 0; i < noOfSamplesPerRegion; ++i) {
		// soundCursor index / halfPeriod = sample * cycles/sample => cycle no and if it's even populate positive amplitude, else populate negative amplitude
		int16_t sampleVal = GetSoundSample(properties, waveType);
		*bufferRegion++ = sampleVal; // LEFT channel sound data
		*bufferRegion++ = sampleVal; // RIGHT channel sound data
		properties->soundCursor = (properties->soundCursor + 1) % totalSamples;
	}
}
