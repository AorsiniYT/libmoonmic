/**
 * @file main.cpp - MINIMAL GUARDIAN FOR TESTING
 * This is a stripped-down version to test if the executable runs at all
 */

#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MessageBoxA(NULL, 
                "Guardian is running!\n\nThis is a test to verify the executable works.",
                "MoonMic Guardian - ALIVE",
                MB_OK | MB_ICONINFORMATION);
    return 0;
}
