#include <windows.h>

int WinMain(HINSTANCE hInstance, 
			HINSTANCE hPrevInstance, 
			LPSTR lpCmdLine, 
			int nCmdShow)
{
	MessageBoxA(NULL, "My Handmade Hero!", "Handmade Hero", MB_OK | MB_ICONINFORMATION);
	return 0;
}