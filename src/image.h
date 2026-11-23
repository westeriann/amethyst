#pragma once
// Decode PNG/JPEG bytes to RGBA8 (top row first). Free with imageFree().
unsigned char* imageDecodeRGBA(const unsigned char* data, int size, int* w, int* h);
void imageFree(unsigned char* px);
