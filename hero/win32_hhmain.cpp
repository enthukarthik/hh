#include <windows.h>
#include <stdint.h>

#define BYTES_PER_PIXEL 4
#define MEMORYRGB(r, g, b) ((r) << 16 | (g) << 8 | (b))

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.

BITMAPINFO BitmapInfo;
void* BitmapMemory;

void
FillColorsInBitmapMemory(
	int width, 
	int height)
{
	uint32_t* pixel = (uint32_t*) BitmapMemory;
	for(int row = 0; row < height; ++row) {
		for(int col = 0; col < width; ++col) {
			*(pixel++) = MEMORYRGB(0, 0, 255);
		}
	}
}

void
CreateOrResizeBitmapMemory(
	int width,
	int height
)
{
	if(BitmapMemory != NULL) {
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapMemory = VirtualAlloc(
		NULL,
		width * height * BYTES_PER_PIXEL, // Assuming 4 bytes per pixel (32 bits per pixel)
		MEM_COMMIT | MEM_RESERVE,
		PAGE_READWRITE
	);

	FillColorsInBitmapMemory(width, height);
}

void
BlitBitmapToWindow(
	HDC hdc,
	int width,
	int height
)
{
	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = width;
	BitmapInfo.bmiHeader.biHeight = -height; // Negative height to indicate a top-down DIB
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = BYTES_PER_PIXEL * 8; // Assuming 32 bits per pixel
	BitmapInfo.bmiHeader.biCompression = BI_RGB; // No compression

	StretchDIBits(
		hdc,
		0, 0, width, height,
		0, 0, width, height,
		BitmapMemory,			// Bitmap memory that contains the color info
		&BitmapInfo,			// BitmapInfo that describes the format of the bitmap memory
		DIB_RGB_COLORS,
		SRCCOPY
	);
}

void
RenderBitmapToWindow(
	HDC hdc,
	int width,
	int height
)
{
	FillColorsInBitmapMemory(width, height);
	BlitBitmapToWindow(hdc, width, height);
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
			int width = LOWORD(lParam);
			int height = HIWORD(lParam);
			CreateOrResizeBitmapMemory(width, height);
		}
		break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			int width = ps.rcPaint.right - ps.rcPaint.left;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;

			RenderBitmapToWindow(hdc, width, height);

			EndPaint(hWnd, &ps);
		}
		break;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
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
	for (;;) {
		MSG msg;
		BOOL bRet = GetMessage(&msg, NULL, 0, 0);
		if (bRet == -1) {
			MessageBox(NULL, TEXT("GetMessage failed with -1"), TEXT("Error"), MB_OK | MB_ICONERROR);
			return;
		} else if (bRet == 0) { // WM_QUIT received
			break;
		} else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

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
		GameLoop();
	}

	return 0;
}
