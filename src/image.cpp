// PNG/JPEG decoding for overlay images. Kept in its own file: this stb_image variant
// declares DXGI names that clash with the DirectX headers used by Spout.
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char* imageDecodeRGBA(const unsigned char* data, int size, int* w, int* h) {
    int n = 0;
    return stbi_load_from_memory(data, size, w, h, &n, 4);
}

void imageFree(unsigned char* px) { stbi_image_free(px); }
