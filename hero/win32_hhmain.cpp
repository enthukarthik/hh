#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#define BYTES_PER_PIXEL 4
// 32 bit ARGB format, should have Alpha followed by RGB. In memory it'll be written as BGRA due to little endian architecture
#define MEMORYRGB(r, g, b) ((r) << 16 | (g) << 8 | (b))

static BITMAPINFO g_bitmapInfo;
static void* g_bitmapMemory;
static int g_bitmapWidth;
static int g_bitmapHeight;
static bool g_gameRunning;

static void
InitializeBitmapInfo()
{
	// Describe the memory layout of the bitmap
	g_bitmapInfo.bmiHeader.biSize = sizeof(g_bitmapInfo.bmiHeader);
	g_bitmapInfo.bmiHeader.biPlanes = 1;
	g_bitmapInfo.bmiHeader.biBitCount = BYTES_PER_PIXEL * 8; // Assuming 32 bits per pixel
	g_bitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression
	// Other fields of BitmapInfoHeader can be left as zero for a simple uncompressed bitmap
}

static void
InitializeGame()
{
	InitializeBitmapInfo();
}

static void
GetClientWidthAndHeight(
	HWND hWnd, 
	int* width, 
	int* height
)
{
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	*width = clientRect.right - clientRect.left;
	*height = clientRect.bottom - clientRect.top;
}

static void
FillColorsInBitmapMemory()
{
	uint32_t* pixel = (uint32_t*) g_bitmapMemory;
	for(int row = 0; row < g_bitmapHeight; ++row) {
		for(int col = 0; col < g_bitmapWidth; ++col) {
			uint8_t red = (uint8_t) col;
			uint8_t green = (uint8_t) row;
			uint8_t blue = (uint8_t) 0;
			*(pixel++) = MEMORYRGB(red, green, blue);
		}
	}
}

static void
CreateNewBitmapMemory(
	int width,
	int height
)
{
	// Free the old bitmap memory if it exists
	if(g_bitmapMemory != NULL) {
		VirtualFree(g_bitmapMemory, 0, MEM_RELEASE);
	}

	g_bitmapWidth = width;
	g_bitmapHeight = height;
	g_bitmapInfo.bmiHeader.biWidth = g_bitmapWidth;
	g_bitmapInfo.bmiHeader.biHeight = -g_bitmapHeight; // Negative height to indicate a top-down DIB

	g_bitmapMemory = VirtualAlloc(
		NULL,
		g_bitmapWidth * g_bitmapHeight * BYTES_PER_PIXEL, // Assuming 4 bytes per pixel (32 bits per pixel)
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);
}

static void
BlitBitmapToWindow(
	HDC hdc,
	HWND hWnd
)
{

	int windowWidth, windowHeight;
	GetClientWidthAndHeight(hWnd, &windowWidth, &windowHeight);

	StretchDIBits(
		hdc,
		0, 0, windowWidth, windowHeight,
		0, 0, g_bitmapWidth, g_bitmapHeight,
		g_bitmapMemory,			// Bitmap memory that contains the color info
		&g_bitmapInfo,			// BitmapInfo that describes the format of the bitmap memory
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

static void
RenderBitmapToWindow(
	HWND hWnd
)
{
	FillColorsInBitmapMemory();

	HDC hDC = GetDC(hWnd);
	BlitBitmapToWindow(hDC, hWnd);
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
		case WM_SIZE:
		{
			int width, height;
			GetClientWidthAndHeight(hWnd, &width, &height);
			CreateNewBitmapMemory(width, height);
		}
		break;

		case WM_PAINT:
		{
			RenderBitmapToWindow(hWnd);
		}
		break;

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

void GameLoop()
{
	g_gameRunning = true;

	while(g_gameRunning) {
		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if(msg.message == WM_QUIT) {
				g_gameRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//Render game code here
	}
}

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.

int APIENTRY
WinMain(
	HINSTANCE hInstance, 
	HINSTANCE, 
	PSTR szCmdLine, 
	int iCmdShow
)
{
	UNREFERENCED_PARAMETER(szCmdLine);
	UNREFERENCED_PARAMETER(iCmdShow);

	HWND gameWindow = CreateGameWindow(hInstance);

	if (gameWindow != NULL) {
		InitializeGame();
		GameLoop();
	}

	return 0;
}
