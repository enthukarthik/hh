#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

// 32 bit color is expected to be in ARGB format. In the memory it'll be written as BGRA due to little endian architecture
#define MEMRGB(r, g, b) (((uint32_t)(r)) << 16 | ((uint32_t)(g)) << 8 | (uint32_t)(b))

static bool g_gameRunning;
struct BackBuffer {
	BITMAPINFO bitmapInfo;
	void* bitmapMemory;
	uint32_t bitmapWidth;
	uint32_t bitmapHeight;
	uint32_t bytesPerPixel;
} g_backBuffer;

struct RectDimension
{
	uint32_t width;
	uint32_t height;
};

static void
InitializeBitmapInfo()
{
	// Describe the memory layout of the bitmap
	g_backBuffer.bytesPerPixel = 4; // Assuming 4 bytes per pixel (xRGB format)
	g_backBuffer.bitmapInfo.bmiHeader.biSize = sizeof(g_backBuffer.bitmapInfo.bmiHeader);
	g_backBuffer.bitmapInfo.bmiHeader.biPlanes = 1; // Must be set to 1
	g_backBuffer.bitmapInfo.bmiHeader.biBitCount = (uint16_t)g_backBuffer.bytesPerPixel * 8; // Assuming 32 bits per pixel
	g_backBuffer.bitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression
	// Other fields of BitmapInfoHeader can be left as zero for a simple uncompressed bitmap
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
FillColorsInBitmapMemory(
	BackBuffer buffer,
	bool animate
)
{
	static uint8_t colorOffset = 0;

	uint32_t* pixel = (uint32_t*) buffer.bitmapMemory;
	for(uint32_t row = 0; row < buffer.bitmapHeight; ++row) {
		for(uint32_t col = 0; col < buffer.bitmapWidth; ++col) {
			uint8_t red   = (uint8_t) (col + colorOffset);
			uint8_t green = (uint8_t) (row + colorOffset);
			uint8_t blue  = (uint8_t) (row + col + colorOffset);
			*(pixel++) = MEMRGB(red, green, blue);
		}
	}

	if(animate) {
		colorOffset += 3;
	}
}

static void
CreateNewBitmapMemory(
	BackBuffer* buffer,
	uint32_t width,
	uint32_t height
)
{
	// Free the old bitmap memory if it exists
	if(buffer->bitmapMemory != NULL) {
		VirtualFree(buffer->bitmapMemory, 0, MEM_RELEASE);
	}

	buffer->bitmapWidth = width;
	buffer->bitmapHeight = height;
	buffer->bitmapInfo.bmiHeader.biWidth = width;
	buffer->bitmapInfo.bmiHeader.biHeight = -((int32_t)height); // Negative height to indicate a top-down DIB. Casting to int32_t to avoid implicit conversion to unsigned type.

	buffer->bitmapMemory = VirtualAlloc(
		NULL,
		(uint64_t)(buffer->bitmapWidth * buffer->bitmapHeight * buffer->bytesPerPixel), // Assuming 4 bytes per pixel (32 bits per pixel)
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
}

static void
InitializeGame()
{
	g_gameRunning = true;
	InitializeBitmapInfo();
	CreateNewBitmapMemory(&g_backBuffer, 1920, 1080); // Initial size of the back buffer bitmap memory
}

static void
BlitBitmapToWindow(
	BackBuffer buffer,
	HDC hdc,
	HWND hWnd
)
{
	RectDimension window = GetClientWindowDimensions(hWnd);

	StretchDIBits(
		hdc,
		0, 0, window.width, window.height,
		0, 0, buffer.bitmapWidth, buffer.bitmapHeight,
		buffer.bitmapMemory,			// Bitmap memory that contains the color info
		&buffer.bitmapInfo,			// BitmapInfo that describes the format of the bitmap memory
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

static void
RenderBitmapToWindow(
	BackBuffer buffer,
	HWND hWnd,
	bool animate
)
{
	FillColorsInBitmapMemory(buffer, animate);

	HDC hDC = GetDC(hWnd);
	BlitBitmapToWindow(buffer, hDC, hWnd);
	ReleaseDC(hWnd, hDC);
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
	wc.lpszClassName = TEXT("HandmadeHeroWindowClass");

	if (RegisterClassEx(&wc) != 0) {
		return CreateWindowEx(
			0,
			wc.lpszClassName,
			TEXT("Handmade Hero"),
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

		RenderBitmapToWindow(g_backBuffer, window, true);
	}
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

	InitializeGame();

	HWND gameWindow = CreateGameWindow(hInstance);

	if (gameWindow != NULL) {
		GameLoop(gameWindow);
	}

	return 0;
}
