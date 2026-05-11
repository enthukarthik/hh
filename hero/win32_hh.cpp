#include "hh.cpp"

#include <windows.h>
#include <stdio.h>
#include <Xinput.h>
#include <dsound.h>

DWORD(*DynXInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD XInputGetStateStub(DWORD, XINPUT_STATE*) { return ERROR_DEVICE_NOT_CONNECTED; }

DWORD(*DynXInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION*) { return ERROR_DEVICE_NOT_CONNECTED; }

typedef HRESULT WINAPI DynDirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
static DynDirectSoundCreate* g_fnDirectSoundCreate;

struct Win32_GameBackBuffer 
{
	struct GameBackBuffer src;
	struct GameBackBuffer dest;
	BITMAPINFO destBitmapInfo;
};

struct Win32_GameState
{
	uint32_t handledXInputPacket;
	int64_t qpFrequency;				// cycles/sec
	int64_t qpCounterOld;				// micro second elapsed
	int64_t qpCounterCurrent;			// micro second elapsed
	int64_t qpCounterDiff;
	int64_t cycleCounterOld;			// clock cycle elapsed
	int64_t cycleCounterCurrent;		// clock cycle elapsed
	int64_t cycleCounterDiff;
	float   msPerFrame;
	float   fps;
};

struct Win32_GameSoundBuffer
{
	struct IDirectSoundBuffer* soundBuffer;
	struct GameSoundBuffer prop;
	bool isSoundPlaying;
};

static bool                         g_gameRunning;
static HDC                          g_win32DC;
static struct Win32_GameBackBuffer  g_win32BackBuffer;
static struct Win32_GameSoundBuffer g_win32SoundBuffer;
static struct Win32_GameState       g_gameState;

static void
Win32_InitializeBitmapInfo()
{
	g_win32BackBuffer.src.bytesPerPixel = 3; // usual bmp file allows 24-bit bmp data

	g_win32BackBuffer.dest.bytesPerPixel = 4;
	g_win32BackBuffer.destBitmapInfo.bmiHeader.biSize = sizeof(g_win32BackBuffer.destBitmapInfo.bmiHeader);
	g_win32BackBuffer.destBitmapInfo.bmiHeader.biPlanes = 1; // Must be set to 1
	g_win32BackBuffer.destBitmapInfo.bmiHeader.biBitCount = (uint16_t) g_win32BackBuffer.dest.bytesPerPixel * 8;
	g_win32BackBuffer.destBitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression
}

static void
Win32_AllocateGameBackBuffer(
	Win32_GameBackBuffer* buffer,
	uint32_t width,
	uint32_t height,
	bool topDownDIB
)
{
	// Free the old bitmap memory if it exists
	if(buffer->dest.memory != NULL) {
		VirtualFree(buffer->dest.memory, 0, MEM_RELEASE);
	}

	buffer->dest.width = width;
	buffer->dest.height = height;
	buffer->destBitmapInfo.bmiHeader.biWidth = width;
	if(topDownDIB) {
		buffer->destBitmapInfo.bmiHeader.biHeight = -((int32_t) height); // Negative height to indicate a top-down DIB. Casting to int32_t to avoid implicit conversion to unsigned type to unsigned.
	} else {
		buffer->destBitmapInfo.bmiHeader.biHeight = height;
	}

	buffer->dest.memory = VirtualAlloc(
		NULL,
		(uint64_t) (buffer->dest.width * buffer->dest.height * buffer->dest.bytesPerPixel),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
}

static void
Win32_InitializeSoundInfo()
{
	g_win32SoundBuffer.prop = InitializeSoundProperties();
	g_win32SoundBuffer.isSoundPlaying = false;
}

static void
Win32_AllocateSoundBuffer(HWND gameWindow, Win32_GameSoundBuffer* buffer)
{
		// Step 3 : Create the DirectSound object
	LPDIRECTSOUND dsoundObj;
	if(g_fnDirectSoundCreate && SUCCEEDED(g_fnDirectSoundCreate(NULL, &dsoundObj, 0))) { // DirectSoundCreate function populate the structure into the dsoundObj
		WAVEFORMATEX waveFormat    = {0};
		waveFormat.wFormatTag      = WAVE_FORMAT_PCM; // Uncompressed Pulse Code Modulation format
		waveFormat.nChannels	   = g_win32SoundBuffer.prop.noOfChannels;
		waveFormat.nSamplesPerSec  = buffer->prop.samplesPerSecond;
		waveFormat.wBitsPerSample  = g_win32SoundBuffer.prop.bitsPerSample;
		waveFormat.nBlockAlign	   = waveFormat.nChannels * waveFormat.wBitsPerSample / 8; // As per documentation
		waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign; // As per documentation
		waveFormat.cbSize		   = 0;

		// Step 4 : SetCooperativeLevel to priority and Create the primary buffer
		if(SUCCEEDED(dsoundObj->SetCooperativeLevel(gameWindow, DSSCL_PRIORITY))) {
			DSBUFFERDESC dsBufferDescription = {0};
			dsBufferDescription.dwSize		 = sizeof(DSBUFFERDESC);
			dsBufferDescription.dwFlags      = DSBCAPS_PRIMARYBUFFER;

			// Step 5 : Create Primary Handle to the sound device and set the format of the audio to play.
			// This is not a buffer and all samples are populated into the buffer which we call as "Secondary buffer"
			LPDIRECTSOUNDBUFFER dsPrimaryBuffer;
			if(SUCCEEDED(dsoundObj->CreateSoundBuffer(&dsBufferDescription, &dsPrimaryBuffer, 0))) {
				// Set for format to play on the sound device
				if(SUCCEEDED(dsPrimaryBuffer->SetFormat(&waveFormat))) {
					//OutputDebugString(TEXT("AllocateSoundBuffer : Primary Buffer set\n"));
				}
			}
		}

		// Step 6 : Create Secondary Buffer
		DSBUFFERDESC dsBufferDescription  = {0};
		dsBufferDescription.dwSize        = sizeof(DSBUFFERDESC);
		dsBufferDescription.dwFlags       = 0;
		dsBufferDescription.dwBufferBytes = buffer->prop.samplesPerSecond * buffer->prop.bufferLengthInSec * buffer->prop.noOfChannels * buffer->prop.bitsPerSample / 8;
		dsBufferDescription.lpwfxFormat   = &waveFormat;

		if(SUCCEEDED(dsoundObj->CreateSoundBuffer(&dsBufferDescription, &g_win32SoundBuffer.soundBuffer, 0))) {
			//OutputDebugString(TEXT("AllocateSoundBuffer : Secondary Buffer set\n"));
		}
	}
}

static void
Win32_LoadGameAssets()
{
	HANDLE p = LoadImageA(NULL, "AP.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	BITMAP file;
	if(p) {
		GetObject(p, sizeof(BITMAP), &file);

		// Describe the memory layout of the bitmap
		g_win32BackBuffer.src.memory = file.bmBits;
		g_win32BackBuffer.src.width= file.bmWidth;
		g_win32BackBuffer.src.height = file.bmHeight;

		Win32_InitializeBitmapInfo();
		Win32_AllocateGameBackBuffer(&g_win32BackBuffer, file.bmWidth, file.bmHeight, false);
	} else {
		Win32_InitializeBitmapInfo();
		Win32_AllocateGameBackBuffer(&g_win32BackBuffer, 1920, 1080, true); // Default back buffer size
	}
}

static void
Win32_LoadXInputLibrary()
{
	// Load XInput library and get the addresses of the XInput functions
	HMODULE xinputLib = LoadLibrary(TEXT("xinput1_4.dll"));
	if(!xinputLib) {
		xinputLib = LoadLibrary(TEXT("xinput1_3.dll"));
	}
	if(!xinputLib) {
		xinputLib = LoadLibrary(TEXT("xinput9_1_0.dll"));
	}
	if(xinputLib) {
		DynXInputGetState = (DWORD (*)(DWORD, XINPUT_STATE*)) GetProcAddress(xinputLib, "XInputGetState");
		DynXInputSetState = (DWORD (*)(DWORD, XINPUT_VIBRATION*)) GetProcAddress(xinputLib, "XInputSetState");
	} else {
		DynXInputGetState = &XInputGetStateStub;
		DynXInputSetState = &XInputSetStateStub;
	}
}

static void
Win32_LoadDirectSoundLibrary(HWND gameWindow, Win32_GameSoundBuffer* buffer)
{
	// Step 1 : Load the dsound library
	HMODULE dsoundLib = LoadLibrary(TEXT("dsound.dll")); // Load the DirectSound dll dynamically
	if(dsoundLib) {
		// Step 2 : Load the DirectSoundCreate proc address
		g_fnDirectSoundCreate = (DynDirectSoundCreate *) GetProcAddress(dsoundLib, "DirectSoundCreate"); // Load the address of the DirectSoundCreate procedure

		Win32_AllocateSoundBuffer(gameWindow, buffer);
	}
}

static void
Win32_LoadGameLibraries(HWND gameWindow)
{
	Win32_LoadXInputLibrary();
	Win32_LoadDirectSoundLibrary(gameWindow, &g_win32SoundBuffer);
}

static void
Win32_InitializeGameTime()
{
	LARGE_INTEGER qpFrequency;
	QueryPerformanceFrequency(&qpFrequency);
	g_gameState.qpFrequency = qpFrequency.QuadPart; // The value of the frequency in which perf counter gets updated in Windows

	LARGE_INTEGER oldCounter;
	QueryPerformanceCounter(&oldCounter);
	g_gameState.qpCounterOld = oldCounter.QuadPart; // The value of high resolution time stamp in microsecond precision

	g_gameState.cycleCounterOld = __rdtsc();
}

static void
InitializeGame(HWND gameWindow)
{
	Win32_InitializeGameTime();
	Win32_InitializeSoundInfo();
	Win32_LoadGameLibraries(gameWindow);
	Win32_LoadGameAssets();
	SetAnimationState();
	g_gameRunning = true;
}

static void
Win32_FillSoundBuffer(
	Win32_GameSoundBuffer* buffer,
	enum SoundWave waveType
)
{
	uint32_t bytesPerSample = buffer->prop.bitsPerSample / 8;
	uint32_t bufferSizeInBytes = buffer->prop.samplesPerSecond * buffer->prop.bufferLengthInSec * buffer->prop.noOfChannels * buffer->prop.bitsPerSample / 8;

	unsigned long cursorPlayPosition = 0;
	unsigned long cursorWritePosition = 0;
	if(SUCCEEDED(buffer->soundBuffer->GetCurrentPosition(&cursorPlayPosition, &cursorWritePosition))) {
		uint32_t soundCursorByte = buffer->prop.soundCursor * bytesPerSample * buffer->prop.noOfChannels; // On each soundCursor index we're writing 4 bytes. 2 bytes for LEFT channel, 2 bytes for RIGHT channel
		uint32_t sizeOfBufferInBytesToLock = 0;
		if(cursorPlayPosition < soundCursorByte) {
			// If play position is before our running sample index, then bytes to lock is
			// from current running sample index to the end of the buffer
			// and start of the buffer to the play position
			sizeOfBufferInBytesToLock = bufferSizeInBytes - soundCursorByte;
			sizeOfBufferInBytesToLock += cursorPlayPosition;
		} else {
			// If play position is after our running sample index, then bytes to lock is
			// from running sample index to the play position
			sizeOfBufferInBytesToLock = cursorPlayPosition - soundCursorByte;
		}
		void* region1;
		unsigned long region1SizeInBytes;
		void* region2;
		unsigned long region2SizeInBytes;

		if(SUCCEEDED(buffer->soundBuffer->Lock(soundCursorByte, sizeOfBufferInBytesToLock,
											   &region1, &region1SizeInBytes,
											   &region2, &region2SizeInBytes, 0))) {
											   // dwFlags in Lock = DSBLOCK_FROMWRITECURSOR ignores dwOffset or DSBLOCK_ENTIREBUFFER ignores sizeOfBufferToLock. We don't want both

			FillSoundBuffer(waveType, region1, region1SizeInBytes, &buffer->prop);
			FillSoundBuffer(waveType, region2, region2SizeInBytes, &buffer->prop);

			buffer->soundBuffer->Unlock(region1, region1SizeInBytes, region2, region2SizeInBytes);
		}
	}
}

static void
Win32_CopyBackBufferToWindow(
	Win32_GameBackBuffer* buffer,
	HWND hWnd
)
{
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	uint32_t width = clientRect.right - clientRect.left;
	uint32_t height = clientRect.bottom - clientRect.top;

	StretchDIBits(
		g_win32DC,
		0, 0, width, height,
		0, 0, buffer->dest.width, buffer->dest.height,
		buffer->dest.memory,			// Bitmap memory that contains the color info
		&buffer->destBitmapInfo,			// BitmapInfo that describes the format of the bitmap memory
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

static void
Win32_PlaySoundBuffer(
	Win32_GameSoundBuffer* buffer
)
{
	if(!buffer->isSoundPlaying) {
		buffer->soundBuffer->Play(0, 0, DSBPLAY_LOOPING);
		buffer->isSoundPlaying = true;
	}
}

static void
RenderGame(
	Win32_GameBackBuffer* buffer,
	Win32_GameSoundBuffer* soundBuffer,
	HWND hWnd
)
{
	FillColorsInBackBuffer(&buffer->dest);
	Win32_CopyBackBufferToWindow(buffer, hWnd);

	Win32_FillSoundBuffer(soundBuffer, SINE_WAVE);
	Win32_PlaySoundBuffer(soundBuffer);
}

static void
Win32_HandleXboxControllerInput()
{
	//Get Xbox Controller Input
	for(uint32_t controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
		XINPUT_STATE controllerState;
		ZeroMemory(&controllerState, sizeof(controllerState));
		if(DynXInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {

			if(controllerState.dwPacketNumber != g_gameState.handledXInputPacket) {
				bool buttonA = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
				bool buttonB = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B;
				//bool buttonX = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_X;
				//bool buttonY = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_Y;

				bool dpadUp	   = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
				bool dpadDown  = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
				bool dpadLeft  = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
				bool dpadRight = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

				bool leftShoulder  = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
				bool rightShoulder = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
#if 0
				int16_t leftThumbX = controllerState.Gamepad.sThumbLX;
				int16_t leftThumbY = controllerState.Gamepad.sThumbLY;
				int16_t rightThumbX = controllerState.Gamepad.sThumbRX;
				int16_t rightThumbY = controllerState.Gamepad.sThumbRY;
#endif
				if(buttonA || buttonB || rightShoulder) {
					ToggleAnimation();
				}

				if(dpadUp || dpadLeft || leftShoulder) {
					ChangeAnimationIncrementValues(1); // Increment the animation value by 1
				}

				if(dpadDown || dpadRight) {
					ChangeAnimationIncrementValues(-1); // Decrement the animation value by 1
				}

				g_gameState.handledXInputPacket = controllerState.dwPacketNumber; // Update the handled packet number to avoid processing the same input multiple times
			}
		}
	}
}

static void
Win32_HandleKeyboardInput(WPARAM wParam, LPARAM lParam)
{
	//bool prevKeyState = (lParam & (1ul << 30)) != 0; // 30th bit is 0 if the key was previously up, 1 if it was previously down. The variable is true if the key was previously down, false if it was previously up.
	bool isKeyPressed = (lParam & (1ul << 31)) == 0; // 31st bit is always 0 for key down messages. The variable is true if the key is currently down, false if it is currently up.

	if(isKeyPressed) {
		switch(wParam) {
			case VK_UP:
			case 'W':
			{
				SetAnimationIncrementValue(0, 0);
				SetAnimationIncrementValue(1, 3); // Increase y offset color
				SetAnimationIncrementValue(2, 0);
			}
			break;

			case VK_DOWN:
			case 'S':
			{
				SetAnimationIncrementValue(0, 0);
				SetAnimationIncrementValue(1, -3); // Decrease y offset color
				SetAnimationIncrementValue(2, 0);
			}
			break;

			case VK_LEFT:
			case 'A':
			{
				SetAnimationIncrementValue(0, 3); // Increase x offset color
				SetAnimationIncrementValue(1, 0);
				SetAnimationIncrementValue(2, 0);
			}
			break;

			case VK_RIGHT:
			case 'D':
			{
				SetAnimationIncrementValue(0, -3); // Decrease x offset color
				SetAnimationIncrementValue(1, 0);
				SetAnimationIncrementValue(2, 0);
			}
			break;

			case VK_ESCAPE:
			{
				g_gameRunning = false;
			}
			break;

			case VK_F4:
			{
				bool altPressed = (lParam & (1ul << 29)) != 0; // 29th bit is 1 if the ALT key is pressed, 0 if it is not pressed. The variable is true if the ALT key is currently pressed, false if it is not currently pressed.
				if(altPressed) {
					g_gameRunning = false;
				}
			}
			break;

			case VK_SPACE:
			{
				ToggleAnimation();
			}
			break;

			default:
				break;
		}
	}
}

static void
PrintDebugString(
	const char* szFormat,
	...
)
{
	char buffer[256] = {0};
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]), szFormat, pArgList);
	va_end(pArgList);

	OutputDebugStringA(buffer);
}

static void 
PrintGameStats()
{
	float megaCyclesElapsed = g_gameState.cycleCounterDiff / (1000.0f * 1000.0f);
	PrintDebugString("%.2f ms/f, %.2f fps, %.2f mc/f\n", g_gameState.msPerFrame, g_gameState.fps, megaCyclesElapsed);
}

static void
CalculateGameStats()
{
	int64_t cycleCountPerSec     = g_gameState.qpFrequency;   // cycle counts/sec. Constant value and cached during game start
	int64_t perfCounterElapsed   = g_gameState.qpCounterDiff; // cycle counts per a single frame rendered in the game loop

	// Perf counter would increase cycleCounts in a second, so for perfCounterElapsed cycles how much seconds would have passed?
	float secPerFrame = (float) perfCounterElapsed / (float) cycleCountPerSec; // cycle counts * sec/cycle counts => seconds. float typecast to avoid int division to zero
	g_gameState.msPerFrame = secPerFrame * 1000;
	g_gameState.fps = (float) cycleCountPerSec / (float) perfCounterElapsed; // (cycle count / sec) * (frame / cycle count) => frame/sec
}

static void 
Win32_UpdateGameTimeInfo()
{
	LARGE_INTEGER currentCounter;
	QueryPerformanceCounter(&currentCounter);
	g_gameState.qpCounterCurrent = currentCounter.QuadPart;
	g_gameState.qpCounterDiff = g_gameState.qpCounterCurrent - g_gameState.qpCounterOld;
	g_gameState.qpCounterOld = g_gameState.qpCounterCurrent;

	g_gameState.cycleCounterCurrent = __rdtsc();
	g_gameState.cycleCounterDiff = g_gameState.cycleCounterCurrent - g_gameState.cycleCounterOld;
	g_gameState.cycleCounterOld = g_gameState.cycleCounterCurrent;

	CalculateGameStats();
	PrintGameStats();
}

static void
Win32_GetUserInput()
{
	Win32_HandleXboxControllerInput();
}

LRESULT CALLBACK 
Win32_WndProc(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
)
{
	LRESULT result = 0;

	switch (uMsg) {
		case WM_DESTROY:
		{
			g_gameRunning = false;
		}
		break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Win32_HandleKeyboardInput(wParam, lParam);
		}
		break;

		default:
			result = DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return result;
}

HWND 
Win32_CreateGameWindow(
	HINSTANCE hInstance
)
{
	WNDCLASSEX wc    = {0}; // Initialize the entire structure to zero to avoid uninitialized members
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = Win32_WndProc;
	wc.hInstance     = hInstance;
	wc.lpszClassName = TEXT("PeakHeroWindowClass");

	if (RegisterClassEx(&wc) != 0) {
		return CreateWindowEx(
			0,
			wc.lpszClassName,
			TEXT("Peak Hero"),
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, hInstance, NULL
		);
	} else {
		MessageBox(NULL, TEXT("Failed to register window class!"), TEXT("Error"), MB_OK | MB_ICONERROR);
		return NULL;
	}
}

void 
Win32_MessageLoop(
	HWND window
)
{
	g_win32DC = GetDC(window);
	while(g_gameRunning) {
		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if(msg.message == WM_QUIT) {
				g_gameRunning = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Win32_GetUserInput();
		RenderGame(&g_win32BackBuffer, &g_win32SoundBuffer, window);
		Win32_UpdateGameTimeInfo();
	}
	ReleaseDC(window, g_win32DC);
}

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.

int32_t APIENTRY
WinMain(
	HINSTANCE hInstance, 
	HINSTANCE, 
	PSTR szCmdLine, 
	int32_t iCmdShow
)
{
	UNREFERENCED_PARAMETER(szCmdLine);
	UNREFERENCED_PARAMETER(iCmdShow);

	HWND gameWindow = Win32_CreateGameWindow(hInstance);

	if (gameWindow != NULL) {
		InitializeGame(gameWindow);
		Win32_MessageLoop(gameWindow);
	}

	return 0;
}
