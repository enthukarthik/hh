#include <windows.h>

#pragma warning (disable:28251) // Disable warning about "Inconsistent annotation for 'WinMain' because we don't want to annotate WinMain with SAL annotations.

int APIENTRY
WinMain(HINSTANCE hInstance, 
		HINSTANCE, 
		PSTR szCmdLine, 
		int iCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(szCmdLine);
	UNREFERENCED_PARAMETER(iCmdShow);

	MessageBox(NULL, TEXT("My Handmade Hero!"), TEXT("Handmade Hero"), MB_OK | MB_ICONINFORMATION);
	return 0;
}
