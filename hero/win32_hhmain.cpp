#include <windows.h>
#include <tchar.h>

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.
int CDECL
FormatString(
	TCHAR* szBuffer,
	size_t bufferSize,
	const TCHAR* szFormat,
	...
)
{
	va_list args;
	va_start(args, szFormat);
	int result = _vsntprintf_s(szBuffer, bufferSize, 1024, szFormat, args);
	va_end(args);
	return result;
}

void CDECL
OutputDebugStringFormat(
	const TCHAR* szFormat,
	...
)
{
	TCHAR szBuffer[1024];
	if (FormatString(szBuffer, sizeof(szBuffer)/sizeof(TCHAR), szFormat) > 0) {
		OutputDebugString(szBuffer);
	}
}

int CDECL
MessageBoxFormat(
	const TCHAR* szFormat,
	...
)
{
	TCHAR szBuffer[1024];
	if (FormatString(szBuffer, sizeof(szBuffer)/sizeof(TCHAR), szFormat) > 0) {
		return MessageBox(NULL, szBuffer, TEXT("Game Information"), MB_OK | MB_ICONINFORMATION);
	}
	return 0;
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
		case WM_CREATE:
		{
			OutputDebugString(TEXT("WM_CREATE received in GameWndProc\n"));
		}
		break;

		case WM_SIZE:
		{
			OutputDebugStringFormat(TEXT("WM_SIZE received in GameWndProc: width=%lu, height=%lu\n"), LOWORD(lParam), HIWORD(lParam));
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
			static DWORD rasterOp = WHITENESS;
			OutputDebugStringFormat(TEXT("In WM_PAINT. Invalid RECT: top=%lu, bottom=%lu, right=%lu, left=%lu\n"), ps.rcPaint.top, ps.rcPaint.bottom, ps.rcPaint.right, ps.rcPaint.left);
			OutputDebugStringFormat(TEXT("In WM_PAINT. Painting coord: x=%lu, y=%lu, width=%lu, height=%lu\n"), x, y, width, height);
			PatBlt(hdc, x, y, width, height, rasterOp);

			if(rasterOp == WHITENESS) {
				rasterOp = BLACKNESS;
			} else {
				rasterOp = WHITENESS;
			}

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
