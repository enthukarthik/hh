#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <Xinput.h>
#include <stdio.h>

// 32 bit color is expected to be in ARGB format. In the memory it'll be written as BGRA due to little endian architecture
#define MEMRGB(r, g, b) (((uint32_t)(r)) << 16 | ((uint32_t)(g)) << 8 | (uint32_t)(b))

DWORD(*DynXInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD XInputGetStateStub(DWORD, XINPUT_STATE*) { return ERROR_DEVICE_NOT_CONNECTED; }

DWORD(*DynXInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION*) { return ERROR_DEVICE_NOT_CONNECTED; }

struct GameAssetBuffer {
	void*      srcAssetBuffer;
	uint32_t   srcBitmapWidth;
	uint32_t   srcBitmapHeight;
	uint32_t   srcBytesPerPixel;
	uint32_t   srcPitch;

	BITMAPINFO destBitmapInfo;
	void*      destBackBuffer;
	uint32_t   destBitmapWidth;
	uint32_t   destBitmapHeight;
	uint32_t   destBytesPerPixel;
};

struct RectDimension
{
	uint32_t width;
	uint32_t height;
};

struct AnimationState
{
	bool	 animate;
	uint32_t colorOffset[3];
	uint32_t incrementValue[3];
};

struct GameState
{
	uint32_t handledXInputPacket;
};

static bool                   g_gameRunning;
static HDC                    g_deviceContext;
static struct GameAssetBuffer g_backBuffer;
static struct AnimationState  g_animationState;
static struct GameState       g_gameState;

static void
InitializeBitmapInfo()
{
	g_backBuffer.srcBytesPerPixel = 3; // usual bmp file allows 24-bit bmp data

	g_backBuffer.destBytesPerPixel = 4;
	g_backBuffer.destBitmapInfo.bmiHeader.biSize = sizeof(g_backBuffer.destBitmapInfo.bmiHeader);
	g_backBuffer.destBitmapInfo.bmiHeader.biPlanes = 1; // Must be set to 1
	g_backBuffer.destBitmapInfo.bmiHeader.biBitCount = (uint16_t) g_backBuffer.destBytesPerPixel * 8;
	g_backBuffer.destBitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression

}

static void
AllocateGameBackBuffer(
	GameAssetBuffer* buffer,
	uint32_t width,
	uint32_t height,
	bool topDownDIB
)
{
	// Free the old bitmap memory if it exists
	if(buffer->destBackBuffer != NULL) {
		VirtualFree(buffer->destBackBuffer, 0, MEM_RELEASE);
	}

	buffer->destBitmapWidth = width;
	buffer->destBitmapHeight = height;
	buffer->destBitmapInfo.bmiHeader.biWidth = width;
	if(topDownDIB) {
		buffer->destBitmapInfo.bmiHeader.biHeight = -((int32_t) height); // Negative height to indicate a top-down DIB. Casting to int32_t to avoid implicit conversion to unsigned type to unsigned.
	} else {
		buffer->destBitmapInfo.bmiHeader.biHeight = height;
	}


	buffer->destBackBuffer = VirtualAlloc(
		NULL,
		(uint64_t) (buffer->destBitmapWidth * buffer->destBitmapHeight * buffer->destBytesPerPixel),
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
}

static void
LoadGameAssets()
{
	HANDLE p = LoadImageA(NULL, "AP.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	BITMAP file;
	if(p) {
		GetObject(p, sizeof(BITMAP), &file);

		// Describe the memory layout of the bitmap
		InitializeBitmapInfo();
		g_backBuffer.srcAssetBuffer = file.bmBits;
		g_backBuffer.srcPitch = file.bmWidthBytes;
		g_backBuffer.srcBitmapWidth = file.bmWidth;
		g_backBuffer.srcBitmapHeight = file.bmHeight;

		AllocateGameBackBuffer(&g_backBuffer, file.bmWidth, file.bmHeight, false);

	} else {
		InitializeBitmapInfo();
		AllocateGameBackBuffer(&g_backBuffer, 1920, 1080, true); // Default back buffer size
	}
}

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
LoadXInputLibrary()
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
LoadGameLibraries()
{
	LoadXInputLibrary();
}

static void
InitializeGame()
{
	g_gameRunning = true;
	LoadGameLibraries();
	LoadGameAssets();
	SetAnimationState();
}

static RectDimension
GetClientWindowDimensions(
	HWND hWnd
)
{
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);

	RectDimension d;
	d.width = clientRect.right - clientRect.left;
	d.height = clientRect.bottom - clientRect.top;
	return d;
}

static void
FillColorsInBackBuffer(
	GameAssetBuffer* buffer
)
{
	if(buffer->srcAssetBuffer != NULL) {
		uint8_t* srcPixel = (uint8_t*) buffer->srcAssetBuffer;
		uint32_t* destPixel = (uint32_t*) buffer->destBackBuffer;

		for(uint32_t row = 0; row < buffer->srcBitmapHeight; ++row) {
			uint8_t* rowPtr = srcPixel + row * buffer->srcPitch;
			for(uint32_t col = 0; col < buffer->srcBitmapWidth; ++col) {
				uint8_t* colPtr = rowPtr + col * buffer->srcBytesPerPixel;
				// In memory the format is BGR, due to little endian
				uint8_t blue = (uint8_t) (*(colPtr + 0) + g_animationState.colorOffset[2]);
				uint8_t green = (uint8_t) (*(colPtr + 1) + g_animationState.colorOffset[1]);
				uint8_t red   = (uint8_t) (*(colPtr + 2) + g_animationState.colorOffset[0]);

				*(destPixel++) = MEMRGB(red, green, blue);
			}
		}
	} else {
		uint32_t* destPixel = (uint32_t*) buffer->destBackBuffer;

		for(uint32_t row = 0; row < buffer->destBitmapHeight; ++row) {
			for(uint32_t col = 0; col < buffer->destBitmapWidth; ++col) {
				uint8_t red   = (uint8_t) (col + g_animationState.colorOffset[0]);
				uint8_t green = (uint8_t) (row + g_animationState.colorOffset[1]);
				uint8_t blue  = (uint8_t) (0 + g_animationState.colorOffset[2]);
				*(destPixel++) = MEMRGB(red, green, blue);
			}
		}
	}

	if(g_animationState.animate) {
		for(int i = 0; i < 3; ++i) {
			g_animationState.colorOffset[i] += g_animationState.incrementValue[i];
		}
	}
}

static void
CopyBackBufferToWindow(
	GameAssetBuffer* buffer,
	HWND hWnd
)
{
	RectDimension window = GetClientWindowDimensions(hWnd);

	StretchDIBits(
		g_deviceContext,
		0, 0, window.width, window.height,
		0, 0, buffer->destBitmapWidth, buffer->destBitmapHeight,
		buffer->destBackBuffer,			// Bitmap memory that contains the color info
		&buffer->destBitmapInfo,			// BitmapInfo that describes the format of the bitmap memory
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

static void
RenderBackBufferToWindow(
	GameAssetBuffer* buffer,
	HWND hWnd
)
{
	FillColorsInBackBuffer(buffer);
	CopyBackBufferToWindow(buffer, hWnd);
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

static void
HandleXboxControllerInput()
{
	//Get Xbox Controller Input
	for(uint32_t controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
		XINPUT_STATE controllerState;
		ZeroMemory(&controllerState, sizeof(controllerState));
		if(DynXInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {

			if(controllerState.dwPacketNumber != g_gameState.handledXInputPacket) {
				bool buttonA = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
				bool buttonB = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B;
				bool buttonX = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_X;
				bool buttonY = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_Y;

				bool dpadUp = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
				bool dpadDown = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
				bool dpadLeft = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
				bool dpadRight = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

				bool leftShoulder = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
				bool rightShoulder = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
#if 0
				int16_t leftThumbX = controllerState.Gamepad.sThumbLX;
				int16_t leftThumbY = controllerState.Gamepad.sThumbLY;
				int16_t rightThumbX = controllerState.Gamepad.sThumbRX;
				int16_t rightThumbY = controllerState.Gamepad.sThumbRY;
#endif
				if(buttonA || buttonB || buttonX || buttonY || rightShoulder) {
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
HandleKeyboardInput(WPARAM wParam, LPARAM lParam)
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
GetUserInput()
{
	HandleXboxControllerInput();
}

LRESULT CALLBACK 
GameWndProc(
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
			HandleKeyboardInput(wParam, lParam);
		}
		break;

		default:
			result = DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return result;
}

HWND CreateGameWindow(
	HINSTANCE hInstance
)
{
	WNDCLASSEX wc = {0}; // Initialize the entire structure to zero to avoid uninitialized members
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = GameWndProc;
	wc.hInstance = hInstance;
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

void GameLoop(
	HWND window
)
{
	g_deviceContext = GetDC(window);
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

		GetUserInput();
		RenderBackBufferToWindow(&g_backBuffer, window);
	}
	ReleaseDC(window, g_deviceContext);
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

	HWND gameWindow = CreateGameWindow(hInstance);

	if (gameWindow != NULL) {
		InitializeGame();
		GameLoop(gameWindow);
	}

	return 0;
}
