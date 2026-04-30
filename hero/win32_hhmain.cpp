#include <windows.h>

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.

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
		case WM_CREATE:
		{
			OutputDebugString(TEXT("WM_CREATE received in GameWndProc\n"));
		}
		break;

		case WM_SIZE:
		{
			OutputDebugString(TEXT("WM_SIZE received in GameWndProc\n"));
		}
		break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);

			int x = ps.rcPaint.top;
			int y = ps.rcPaint.left;
			int width = ps.rcPaint.right - ps.rcPaint.left;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;
			PatBlt(hdc, x, y, width, height, WHITENESS);

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
