#pragma once

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../LumeEngine/third_party/stb/stb_image_write.h"

#include <cstdio>
#include <cstdint>
#include <cstring>

// Save RGBA data to PNG file
inline bool SavePNG(const char* filename, int width, int height, const uint8_t* rgba_data) {
    if (!rgba_data || !filename) {
        return false;
    }
    
    int result = stbi_write_png(filename, width, height, 4, rgba_data, width * 4);
    return result != 0;
}

// Save RGB data to PNG file  
inline bool SavePNG_RGB(const char* filename, int width, int height, const uint8_t* rgb_data) {
    if (!rgb_data || !filename) {
        return false;
    }
    
    int result = stbi_write_png(filename, width, height, 3, rgb_data, width * 3);
    return result != 0;
}