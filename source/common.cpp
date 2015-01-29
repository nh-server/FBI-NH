#include <stdio.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/dirent.h>

#include <stack>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <3ds.h>

#include "common.hpp"

static unsigned char asciiData[128][8] = {
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x3E, 0x41, 0x55, 0x41, 0x55, 0x49, 0x3E },
		{ 0x00, 0x3E, 0x7F, 0x6B, 0x7F, 0x6B, 0x77, 0x3E },
		{ 0x00, 0x22, 0x77, 0x7F, 0x7F, 0x3E, 0x1C, 0x08 },
		{ 0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x1C, 0x08 },
		{ 0x00, 0x08, 0x1C, 0x2A, 0x7F, 0x2A, 0x08, 0x1C },
		{ 0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x3E, 0x08, 0x1C },
		{ 0x00, 0x00, 0x1C, 0x3E, 0x3E, 0x3E, 0x1C, 0x00 },
		{ 0xFF, 0xFF, 0xE3, 0xC1, 0xC1, 0xC1, 0xE3, 0xFF },
		{ 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00 },
		{ 0xFF, 0xFF, 0xE3, 0xDD, 0xDD, 0xDD, 0xE3, 0xFF },
		{ 0x00, 0x0F, 0x03, 0x05, 0x39, 0x48, 0x48, 0x30 },
		{ 0x00, 0x08, 0x3E, 0x08, 0x1C, 0x22, 0x22, 0x1C },
		{ 0x00, 0x18, 0x14, 0x10, 0x10, 0x30, 0x70, 0x60 },
		{ 0x00, 0x0F, 0x19, 0x11, 0x13, 0x37, 0x76, 0x60 },
		{ 0x00, 0x08, 0x2A, 0x1C, 0x77, 0x1C, 0x2A, 0x08 },
		{ 0x00, 0x60, 0x78, 0x7E, 0x7F, 0x7E, 0x78, 0x60 },
		{ 0x00, 0x03, 0x0F, 0x3F, 0x7F, 0x3F, 0x0F, 0x03 },
		{ 0x00, 0x08, 0x1C, 0x2A, 0x08, 0x2A, 0x1C, 0x08 },
		{ 0x00, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x66 },
		{ 0x00, 0x3F, 0x65, 0x65, 0x3D, 0x05, 0x05, 0x05 },
		{ 0x00, 0x0C, 0x32, 0x48, 0x24, 0x12, 0x4C, 0x30 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x7F, 0x7F },
		{ 0x00, 0x08, 0x1C, 0x2A, 0x08, 0x2A, 0x1C, 0x3E },
		{ 0x00, 0x08, 0x1C, 0x3E, 0x7F, 0x1C, 0x1C, 0x1C },
		{ 0x00, 0x1C, 0x1C, 0x1C, 0x7F, 0x3E, 0x1C, 0x08 },
		{ 0x00, 0x08, 0x0C, 0x7E, 0x7F, 0x7E, 0x0C, 0x08 },
		{ 0x00, 0x08, 0x18, 0x3F, 0x7F, 0x3F, 0x18, 0x08 },
		{ 0x00, 0x00, 0x00, 0x70, 0x70, 0x70, 0x7F, 0x7F },
		{ 0x00, 0x00, 0x14, 0x22, 0x7F, 0x22, 0x14, 0x00 },
		{ 0x00, 0x08, 0x1C, 0x1C, 0x3E, 0x3E, 0x7F, 0x7F },
		{ 0x00, 0x7F, 0x7F, 0x3E, 0x3E, 0x1C, 0x1C, 0x08 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18 },
		{ 0x00, 0x36, 0x36, 0x14, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36 },
		{ 0x00, 0x08, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x08 },
		{ 0x00, 0x60, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x06 },
		{ 0x00, 0x3C, 0x66, 0x3C, 0x28, 0x65, 0x66, 0x3F },
		{ 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00 },
		{ 0x00, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06 },
		{ 0x00, 0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60 },
		{ 0x00, 0x00, 0x36, 0x1C, 0x7F, 0x1C, 0x36, 0x00 },
		{ 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x60 },
		{ 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60 },
		{ 0x00, 0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00 },
		{ 0x00, 0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C },
		{ 0x00, 0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E },
		{ 0x00, 0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E },
		{ 0x00, 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C },
		{ 0x00, 0x0C, 0x1C, 0x2C, 0x4C, 0x7E, 0x0C, 0x0C },
		{ 0x00, 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C },
		{ 0x00, 0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C },
		{ 0x00, 0x7E, 0x66, 0x0C, 0x0C, 0x18, 0x18, 0x18 },
		{ 0x00, 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C },
		{ 0x00, 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C },
		{ 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00 },
		{ 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30 },
		{ 0x00, 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06 },
		{ 0x00, 0x00, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x00 },
		{ 0x00, 0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60 },
		{ 0x00, 0x3C, 0x66, 0x06, 0x1C, 0x18, 0x00, 0x18 },
		{ 0x00, 0x38, 0x44, 0x5C, 0x58, 0x42, 0x3C, 0x00 },
		{ 0x00, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66 },
		{ 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C },
		{ 0x00, 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C },
		{ 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C },
		{ 0x00, 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E },
		{ 0x00, 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60 },
		{ 0x00, 0x3C, 0x66, 0x60, 0x60, 0x6E, 0x66, 0x3C },
		{ 0x00, 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66 },
		{ 0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C },
		{ 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x6C, 0x6C, 0x38 },
		{ 0x00, 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66 },
		{ 0x00, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E },
		{ 0x00, 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63 },
		{ 0x00, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x63 },
		{ 0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C },
		{ 0x00, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60 },
		{ 0x00, 0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x06 },
		{ 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66 },
		{ 0x00, 0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C },
		{ 0x00, 0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x18 },
		{ 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E },
		{ 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18 },
		{ 0x00, 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63 },
		{ 0x00, 0x63, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x63 },
		{ 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18 },
		{ 0x00, 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E },
		{ 0x00, 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E },
		{ 0x00, 0x00, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00 },
		{ 0x00, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78 },
		{ 0x00, 0x08, 0x14, 0x22, 0x41, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F },
		{ 0x00, 0x0C, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E },
		{ 0x00, 0x60, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C },
		{ 0x00, 0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C },
		{ 0x00, 0x06, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E },
		{ 0x00, 0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C },
		{ 0x00, 0x1C, 0x36, 0x30, 0x30, 0x7C, 0x30, 0x30 },
		{ 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C },
		{ 0x00, 0x60, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66 },
		{ 0x00, 0x00, 0x18, 0x00, 0x18, 0x18, 0x18, 0x3C },
		{ 0x00, 0x0C, 0x00, 0x0C, 0x0C, 0x6C, 0x6C, 0x38 },
		{ 0x00, 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66 },
		{ 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 },
		{ 0x00, 0x00, 0x00, 0x63, 0x77, 0x7F, 0x6B, 0x6B },
		{ 0x00, 0x00, 0x00, 0x7C, 0x7E, 0x66, 0x66, 0x66 },
		{ 0x00, 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C },
		{ 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60 },
		{ 0x00, 0x00, 0x3C, 0x6C, 0x6C, 0x3C, 0x0D, 0x0F },
		{ 0x00, 0x00, 0x00, 0x7C, 0x66, 0x66, 0x60, 0x60 },
		{ 0x00, 0x00, 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x7C },
		{ 0x00, 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x18 },
		{ 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E },
		{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x3C, 0x18 },
		{ 0x00, 0x00, 0x00, 0x63, 0x6B, 0x6B, 0x6B, 0x3E },
		{ 0x00, 0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66 },
		{ 0x00, 0x00, 0x00, 0x66, 0x66, 0x3E, 0x06, 0x3C },
		{ 0x00, 0x00, 0x00, 0x3C, 0x0C, 0x18, 0x30, 0x3C },
		{ 0x00, 0x0E, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0E },
		{ 0x00, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18 },
		{ 0x00, 0x70, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x70 },
		{ 0x00, 0x00, 0x00, 0x3A, 0x6C, 0x00, 0x00, 0x00 },
		{ 0x00, 0x08, 0x1C, 0x36, 0x63, 0x41, 0x41, 0x7F }
};

PAD_KEY buttonMap[13] = {
		KEY_A,
		KEY_B,
		KEY_X,
		KEY_Y,
		KEY_L,
		KEY_R,
		KEY_START,
		KEY_SELECT,
		KEY_UP,
		KEY_DOWN,
		KEY_LEFT,
		KEY_RIGHT,
		KEY_TOUCH
};

u8* fb = NULL;
u16 fbWidth = 0;
u16 fbHeight = 0;

bool screen_begin_draw(Screen screen) {
	if(fb != NULL) {
		return false;
	}

	fb = gfxGetFramebuffer(screen == TOP_SCREEN ? GFX_TOP : GFX_BOTTOM, GFX_LEFT, &fbWidth, &fbHeight);
	return true;
}

bool screen_end_draw() {
	if(fb == NULL) {
		return false;
	}

	fb = NULL;
	fbWidth = 0;
	fbHeight = 0;
	return true;
}

void screen_swap_buffers_quick() {
	gfxFlushBuffers();
	gfxSwapBuffers();
}

void screen_swap_buffers() {
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
}

int screen_get_width() {
	// Use fbHeight since the framebuffer is rotated 90 degrees to the left.
	if(fb != NULL) {
		return fbHeight;
	}

	return 0;
}

int screen_get_height() {
	// Use fbWidth since the framebuffer is rotated 90 degrees to the left.
	if(fb != NULL) {
		return fbWidth;
	}

	return 0;
}

void screen_take_screenshot() {
	u32 imageSize = 400 * 480 * 3;
	u8 temp[0x36 + imageSize];
	memset(temp, 0, 0x36 + imageSize);

	*(u16*) &temp[0x0] = 0x4D42;
	*(u32*) &temp[0x2] = 0x36 + imageSize;
	*(u32*) &temp[0xA] = 0x36;
	*(u32*) &temp[0xE] = 0x28;
	*(u32*) &temp[0x12] = 400;
	*(u32*) &temp[0x16] = 480;
	*(u32*) &temp[0x1A] = 0x00180001;
	*(u32*) &temp[0x22] = imageSize;

	u8* framebuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	for(int y = 0; y < 240; y++) {
		for(int x = 0; x < 400; x++) {
			int si = ((239 - y) + (x * 240)) * 3;
			int di = 0x36 + (x + ((479 - y) * 400)) * 3;
			temp[di++] = framebuf[si++];
			temp[di++] = framebuf[si++];
			temp[di++] = framebuf[si++];
		}
	}

	framebuf = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	for(int y = 0; y < 240; y++) {
		for(int x = 0; x < 320; x++) {
			int si = ((239 - y) + (x * 240)) * 3;
			int di = 0x36 + ((x+40) + ((239 - y) * 400)) * 3;
			temp[di++] = framebuf[si++];
			temp[di++] = framebuf[si++];
			temp[di++] = framebuf[si++];
		}
	}

	char file[256];
	snprintf(file, 256, "sdmc:/screenshot_%08d.bmp", (int) (svcGetSystemTick() / 446872));
	int fd = open(file, O_WRONLY | O_CREAT | O_SYNC);
	write(fd, temp, 0x36 + imageSize);
	close(fd);
}

int screen_get_index(int x, int y) {
	int height = screen_get_height();
	// Reverse the y coordinate when finding the index.
	// This is done as the framebuffer is rotated 90 degrees to the left.
	return ((height - y) + x * height) * 3;
}

void screen_draw(int x, int y, u8 r, u8 g, u8 b) {
	if(fb == NULL || x < 0 || y < 0 || x >= screen_get_width() || y >= screen_get_height()) {
		return;
	}

	int idx = screen_get_index(x, y);
	fb[idx + 0] = b;
	fb[idx + 1] = g;
	fb[idx + 2] = r;
}

void screen_fill(int x, int y, int width, int height, u8 r, u8 g, u8 b) {
	if(fb == NULL) {
		return;
	}

	int swidth = screen_get_width();
	int sheight = screen_get_height();
	if(x + width < 0 || y + height < 0 || x >= swidth || y >= sheight) {
		return;
	}

	if(x < 0) {
		width += x;
		x = 0;
	}

	if(y < 0) {
		height += y;
		y = 0;
	}

	if(x + width >= swidth){
		width = swidth - x;
	}

	if(y + height >= sheight){
		height = sheight - y;
	}

	u8 colorLine[height * 3];
	for(int ly = 0; ly < height; ly++) {
		colorLine[ly * 3 + 0] = b;
		colorLine[ly * 3 + 1] = g;
		colorLine[ly * 3 + 2] = r;
	}

	u8* fbAddr = fb + screen_get_index(x, y) - (height * 3);
	for(int dx = 0; dx < width; dx++) {
		memcpy(fbAddr, colorLine, (size_t) (height * 3));
		fbAddr += sheight * 3;
	}
}

int screen_get_str_width(const std::string str) {
	return str.length() * 8;
}

int screen_get_str_height(const std::string str) {
	return 8;
}

void screen_draw_char(char c, int x, int y, u8 r, u8 g, u8 b) {
	if(fb == NULL) {
		return;
	}

	unsigned char* data = asciiData[(int) c];
	for(int cy = 0; cy < 8; cy++) {
		unsigned char l = data[cy];
		for(int cx = 0; cx < 8; cx++) {
			if((0b10000000 >> cx) & l) {
				screen_draw(x + cx, y + cy, r, g, b);
			}
		}
	}
}

void screen_draw_string(const std::string str, int x, int y, u8 r, u8 g, u8 b) {
	if(fb == NULL) {
		return;
	}

	int len = str.length();
	int cx = x;
	int cy = y;
	for(int i = 0; i < len; i++) {
		char c = str[i];
		if(c == '\n') {
			cx = x;
			cy += 8;
                        continue;
		}

		screen_draw_char(c, cx, cy, r, g, b);
		cx += 8;
	}
}

void screen_clear(u8 r, u8 g, u8 b) {
	screen_fill(0, 0, screen_get_width(), screen_get_height(), r, g, b);
}

typedef struct {
    std::string id;
    std::string name;
    std::vector<std::string> details;
} SelectableElement;

SelectionResult ui_select(std::vector<SelectableElement> elements, SelectableElement* selected, bool enableBack, std::function<bool()> onLoop) {
    u32 cursor = 0;
    u32 scroll = 0;

    u32 selectionScroll = 0;
    u64 selectionScrollEndTime = 0;

    while(platform_is_running()) {
        input_poll();
        if(input_is_pressed(BUTTON_A)) {
            *selected = elements.at(cursor);
            return SELECTED;
        }

        if(enableBack && input_is_pressed(BUTTON_B)) {
            return BACK;
        }

        if(input_is_pressed(BUTTON_DOWN) && cursor < elements.size() - 1) {
            cursor++;
            if(cursor >= scroll + 20) {
                scroll++;
            }

            selectionScroll = 0;
            selectionScrollEndTime = 0;
        }

        if(input_is_pressed(BUTTON_UP) && cursor > 0) {
            cursor--;
            if(cursor < scroll) {
                scroll--;
            }

            selectionScroll = 0;
            selectionScrollEndTime = 0;
        }

        screen_begin_draw(BOTTOM_SCREEN);
        screen_clear(0, 0, 0);

        u32 screenWidth = (u32) screen_get_width();
        for(std::vector<SelectableElement>::iterator it = elements.begin() + scroll; it != elements.begin() + scroll + 20 && it != elements.end(); it++) {
            SelectableElement element = *it;
            u32 index = (u32) (it - elements.begin());
            u8 color = 255;
            int offset = 0;
            if(index == cursor) {
                color = 0;
                screen_fill(0, (int) (index - scroll) * 12, (int) screenWidth, screen_get_str_height(element.name), 255, 255, 255);
                u32 width = (u32) screen_get_str_width(element.name);
                if(width > screenWidth) {
                    if(selectionScroll + screenWidth >= width) {
                        if(selectionScrollEndTime == 0) {
                            selectionScrollEndTime = platform_get_time();
                        } else if(platform_get_time() - selectionScrollEndTime >= 4000) {
                            selectionScroll = 0;
                            selectionScrollEndTime = 0;
                        }
                    } else {
                        selectionScroll++;
                    }
                }

                offset = -selectionScroll;
            }

            screen_draw_string(element.name, offset, (int) (index - scroll) * 12, color, color, color);
        }

        screen_end_draw();

        screen_begin_draw(TOP_SCREEN);
        screen_clear(0, 0, 0);

        SelectableElement currSelected = elements.at(cursor);
        if(currSelected.details.size() != 0) {
            for(std::vector<std::string>::iterator it = currSelected.details.begin(); it != currSelected.details.end(); it++) {
                std::string detail = *it;
                u32 index = (u32) (it - currSelected.details.begin());
                screen_draw_string(detail, 0, (int) index * 12, 255, 255, 255);
            }
        }

        bool result = onLoop();

        screen_end_draw();
        screen_swap_buffers();
        if(result) {
            return MANUAL_BREAK;
        }
    }

    return APP_CLOSING;
}

bool ui_is_directory(const std::string path) {
    DIR *dir = opendir(path.c_str());
    if(!dir) {
        return false;
    }

    closedir(dir);
    return true;
}

struct ui_alphabetize {
    inline bool operator() (SelectableElement a, SelectableElement b) {
        return a.name.compare(b.name) < 0;
    }
};

std::vector<SelectableElement> ui_get_dir_elements(const std::string directory, const std::string extension) {
    std::vector<SelectableElement> elements;
    elements.push_back({".", "."});
    elements.push_back({"..", ".."});

    DIR *dir = opendir(directory.c_str());
    if(dir != NULL) {
        while(true) {
            struct dirent *ent = readdir(dir);
            if(ent == NULL) {
                break;
            }

            const std::string dirName = std::string(ent->d_name);
            const std::string path = directory + "/" + dirName;
            if(ui_is_directory(path)) {
                elements.push_back({path, dirName});
            } else {
                std::string::size_type dotPos = path.rfind('.');
                if(dotPos != std::string::npos && path.substr(dotPos + 1).compare(extension) == 0) {
                    struct stat st;
                    stat(path.c_str(), &st);

                    std::vector<std::string> info;
                    std::stringstream stream;
                    stream << "File Size: " << st.st_size << " bytes (" << std::fixed << std::setprecision(2) << st.st_size / 1024.0f / 1024.0f << "MB)";
                    info.push_back(stream.str());
                    elements.push_back({path, dirName, info});
                }
            }
        }

        closedir(dir);
    }

    std::sort(elements.begin(), elements.end(), ui_alphabetize());
    return elements;
}

bool ui_select_file(const std::string rootDirectory, const std::string extension, std::string* selectedFile, std::function<bool()> onLoop) {
    std::stack<std::string> directoryStack;
    std::string currDirectory = rootDirectory;
    while(platform_is_running()) {
        SelectableElement selected;
        std::vector<SelectableElement> contents = ui_get_dir_elements(currDirectory, extension);
        SelectionResult result = ui_select(contents, &selected, !directoryStack.empty(), onLoop);
        if(result == APP_CLOSING || result == MANUAL_BREAK) {
            break;
        } else if(result == BACK) {
            currDirectory = directoryStack.top();
            directoryStack.pop();
        } else if(result == SELECTED) {
            if(selected.name.compare(".") == 0) {
                continue;
            } else if(selected.name.compare("..") == 0) {
                if(directoryStack.empty()) {
                    continue;
                }

                currDirectory = directoryStack.top();
                directoryStack.pop();
            } else {
                if(ui_is_directory(selected.id)) {
                    directoryStack.push(currDirectory);
                    currDirectory = selected.id;
                } else {
                    *selectedFile = selected.id;
                    return true;
                }
            }
        }
    }

    return false;
}

bool ui_select_app(MediaType mediaType, App* selectedApp, std::function<bool()> onLoop) {
    std::vector<App> apps = app_list(mediaType);
    std::vector<SelectableElement> elements;
    for(std::vector<App>::iterator it = apps.begin(); it != apps.end(); it++) {
        App app = *it;

        std::stringstream titleId;
        titleId << std::setfill('0') << std::setw(16) << std::hex << app.titleId;

        std::stringstream uniqueId;
        uniqueId << std::setfill('0') << std::setw(8) << std::hex << app.uniqueId;

        std::vector<std::string> details;
        details.push_back("Title ID: " + titleId.str());
        details.push_back("Unique ID: " + uniqueId.str());
        details.push_back("Product Code: " + std::string(app.productCode));
        details.push_back("Platform: " + app_get_platform_name(app.platform));
        details.push_back("Category: " + app_get_category_name(app.category));

        elements.push_back({titleId.str(), app.productCode, details});
    }

    if(elements.size() == 0) {
        elements.push_back({"None", "None"});
    }

    std::sort(elements.begin(), elements.end(), ui_alphabetize());

    SelectableElement selected;
    SelectionResult result = ui_select(elements, &selected, false, onLoop);
    if(result != APP_CLOSING && result != MANUAL_BREAK && selected.id.compare("None") != 0) {
        for(std::vector<App>::iterator it = apps.begin(); it != apps.end(); it++) {
            App app = *it;
            if(app.titleId == (u64) strtoll(selected.id.c_str(), NULL, 16)) {
                *selectedApp = app;
            }
        }

        return true;
    }

    return false;
}

void input_poll() {
	hidScanInput();
}

bool input_is_released(Button button) {
	return (hidKeysUp() & buttonMap[button]) != 0;
}

bool input_is_pressed(Button button) {
	return (hidKeysDown() & buttonMap[button]) != 0;
}

bool input_is_held(Button button) {
	return (hidKeysHeld() & buttonMap[button]) != 0;
}

Touch input_get_touch() {
	touchPosition pos;
	hidTouchRead(&pos);

	Touch touch;
	touch.x = pos.px;
	touch.y = pos.py;
	return touch;
}

bool amInitialized = false;
bool nsInitialized = false;

bool am_prepare() {
	if(!amInitialized) {
		if(amInit() != 0) {
			return false;
		}

		amInitialized = true;
	}

	return true;
}

bool ns_prepare() {
	if(!nsInitialized) {
		if(nsInit() != 0) {
			return false;
		}

		nsInitialized = true;
	}

	return true;
}

u8 app_mediatype_to_byte(MediaType mediaType) {
	return mediaType == NAND ? mediatype_NAND : mediatype_SDMC;
}

AppPlatform app_platform_from_id(u16 id) {
	switch(id) {
		case 1:
			return WII;
		case 3:
			return DSI;
		case 4:
			return THREEDS;
		case 5:
			return WIIU;
		default:
			return UNKNOWN_PLATFORM;
	}
}

AppCategory app_category_from_id(u16 id) {
	if((id & 0x2) == 0x2) {
		return DLC;
	} else if((id & 0x6) == 0x6) {
		return PATCH;
	} else if((id & 0x10) == 0x10) {
		return SYSTEM;
	} else if((id & 0x8000) == 0x8000) {
		return TWL;
	}

	return APP;
}

const std::string app_get_platform_name(AppPlatform platform) {
	switch(platform) {
		case WII:
			return "Wii";
		case DSI:
			return "DSi";
		case THREEDS:
			return "3DS";
		case WIIU:
			return "Wii U";
		default:
			return "Unknown";
	}
}

const std::string app_get_category_name(AppCategory category) {
	switch(category) {
		case APP:
			return "App";
		case DLC:
			return "DLC";
		case PATCH:
			return "Patch";
		case SYSTEM:
			return "System";
		case TWL:
			return "TWL";
		default:
			return "Unknown";
	}
}

std::vector<App> app_list(MediaType mediaType) {
	std::vector<App> titles;
	if(!am_prepare()) {
		return titles;
	}

	u32 titleCount;
	if(AM_GetTitleCount(app_mediatype_to_byte(mediaType), &titleCount) != 0) {
		return titles;
	}

	u64 titleIds[titleCount];
	if(AM_GetTitleList(app_mediatype_to_byte(mediaType), titleCount, titleIds) != 0) {
		return titles;
	}

	for(u32 i = 0; i < titleCount; i++) {
		u64 titleId = titleIds[i];
		App app;
		app.titleId = titleId;
		app.uniqueId = ((u32*) &titleId)[0];
		AM_GetTitleProductCode(app_mediatype_to_byte(mediaType), titleId, app.productCode);
		if(strcmp(app.productCode, "") == 0) {
			strcpy(app.productCode, "<N/A>");
		}

		app.mediaType = mediaType;
		app.platform = app_platform_from_id(((u16*) &titleId)[3]);
		app.category = app_category_from_id(((u16*) &titleId)[2]);

		titles.push_back(app);
	}

	return titles;
}

bool app_install(MediaType mediaType, const std::string path, std::function<bool(int)> onProgress) {
	if(!am_prepare()) {
		return false;
	}

	FILE* fd = fopen(path.c_str(), "r");
	if(!fd) {
		return false;
	}

	fseek(fd, 0, SEEK_END);
	u64 size = (u64) ftell(fd);
	fseek(fd, 0, SEEK_SET);

	if(onProgress != NULL) {
		onProgress(0);
	}

	Handle ciaHandle;
	if(AM_StartCiaInstall(app_mediatype_to_byte(mediaType), &ciaHandle) != 0) {
		return false;
	}

	FSFILE_SetSize(ciaHandle, size);

	u32 bufSize = 1024 * 256; // 256KB
    void* buf = malloc(bufSize);
	bool cancelled = false;
	for(u64 pos = 0; pos < size; pos += bufSize) {
		if(onProgress != NULL && !onProgress((int) ((pos / (float) size) * 100))) {
			AM_CancelCIAInstall(&ciaHandle);
			cancelled = true;
			break;
		}

		u32 bytesRead = fread(buf, 1, bufSize, fd);
		FSFILE_Write(ciaHandle, NULL, pos, buf, bytesRead, FS_WRITE_NOFLUSH);
	}

    free(buf);
	fclose(fd);

	if(cancelled) {
		return false;
	}

	if(onProgress != NULL) {
		onProgress(100);
	}

	Result res = AM_FinishCiaInstall(app_mediatype_to_byte(mediaType), &ciaHandle);
	if(res != 0 && (u32) res != 0xC8A044DC) { // Happens when already installed, but seems to have succeeded anyway...
		return false;
	}

	return true;
}

bool app_delete(App app) {
	if(!am_prepare()) {
		return false;
	}

	return AM_DeleteAppTitle(app_mediatype_to_byte(app.mediaType), app.titleId) == 0;
}

bool app_launch(App app) {
	if(!ns_prepare()) {
		return false;
	}

	return NS_RebootToTitle(app.mediaType, app.titleId) == 0;
}

u64 fs_get_free_space(MediaType mediaType) {
	u32 clusterSize;
	u32 freeClusters;
	Result res = 0;
	if(mediaType == NAND) {
		res = FSUSER_GetNandArchiveResource(NULL, NULL, &clusterSize, NULL, &freeClusters);
	} else {
		res = FSUSER_GetSdmcArchiveResource(NULL, NULL, &clusterSize, NULL, &freeClusters);
	}

	if(res != 0) {
		return 0;
	}

	return clusterSize * freeClusters;
}

bool platform_init() {
	if(srvInit() != 0 || aptInit() != 0 || hidInit(NULL) != 0 || fsInit() != 0 || sdmcInit() != 0) {
		return false;
	}

	gfxInitDefault();
	return true;
}

void platform_cleanup() {
	if(amInitialized) {
		amExit();
		amInitialized = false;
	}

	if(nsInitialized) {
		nsExit();
		nsInitialized = false;
	}

	sdmcExit();
	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
}

bool platform_is_running() {
	return aptMainLoop();
}

u64 platform_get_time() {
	return osGetTime();
}

void platform_delay(int ms) {
	svcSleepThread(ms * 1000000);
}

void platform_printf(const char* format, ...) {
	va_list args;
	va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    char str[len + 1];

    va_list args2;
    va_start(args2, format);
    vsnprintf(str, (size_t) len + 1, format, args2);
    va_end(args2);

    svcOutputDebugString(str, strlen(str));
}