#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdio>
#include <cstdint>

// Simple screenshot capture using Windows GDI - captures specific window
inline bool SaveWindowScreenshot(HWND hWnd, const char* filename, int width, int height) {
#ifdef _WIN32
    if (!hWnd) {
        return false;
    }
    
    // Get the window DC (not screen DC)
    HDC hdcWindow = GetDC(hWnd);
    HDC hdcMem = CreateCompatibleDC(hdcWindow);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(hWnd, hdcWindow);
        return false;
    }
    
    SelectObject(hdcMem, hBitmap);
    
    // Copy window content to bitmap
    BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
    
    // Get bitmap data
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    uint8_t* pixels = new uint8_t[width * height * 3];
    GetDIBits(hdcMem, hBitmap, 0, height, pixels, &bmi, DIB_RGB_COLORS);
    
    // Save as PPM (simple format, no library needed)
    FILE* f = fopen(filename, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", width, height);
        fwrite(pixels, 1, width * height * 3, f);
        fclose(f);
    }
    
    delete[] pixels;
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(hWnd, hdcWindow);
    
    return f != nullptr;
#else
    return false;
#endif
}