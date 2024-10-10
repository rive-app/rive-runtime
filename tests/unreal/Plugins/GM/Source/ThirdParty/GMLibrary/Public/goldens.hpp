#pragma once
#include <vector>
class TestingWindow;

extern int /*__declspec(dllimport)*/ goldens_main(int argc, const char* argv[]);
/*void __declspec(dllimport) goldens_resize_pixel_data(std::vector<uint8_t>* pixels, size_t size);
void __declspec(dllimport) goldens_init(TestingWindow* InTestingWindow);
void __declspec(dllimport) goldens_memcpy(void* dst, void* src, size_t size);*/
