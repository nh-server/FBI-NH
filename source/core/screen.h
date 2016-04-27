#pragma once

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TEXTURES 1024

#define TEXTURE_BOTTOM_SCREEN_BG 0
#define TEXTURE_BOTTOM_SCREEN_TOP_BAR 1
#define TEXTURE_BOTTOM_SCREEN_TOP_BAR_SHADOW 2
#define TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR 3
#define TEXTURE_BOTTOM_SCREEN_BOTTOM_BAR_SHADOW 4
#define TEXTURE_TOP_SCREEN_BG 5
#define TEXTURE_TOP_SCREEN_TOP_BAR 6
#define TEXTURE_TOP_SCREEN_TOP_BAR_SHADOW 7
#define TEXTURE_TOP_SCREEN_BOTTOM_BAR 8
#define TEXTURE_TOP_SCREEN_BOTTOM_BAR_SHADOW 9
#define TEXTURE_LOGO 10
#define TEXTURE_SELECTION_OVERLAY 11
#define TEXTURE_BUTTON_SMALL 12
#define TEXTURE_BUTTON_LARGE 13
#define TEXTURE_PROGRESS_BAR_BG 14
#define TEXTURE_PROGRESS_BAR_CONTENT 15
#define TEXTURE_META_INFO_BOX 16
#define TEXTURE_META_INFO_BOX_SHADOW 17
#define TEXTURE_BATTERY_CHARGING 18
#define TEXTURE_BATTERY_0 19
#define TEXTURE_BATTERY_1 20
#define TEXTURE_BATTERY_2 21
#define TEXTURE_BATTERY_3 22
#define TEXTURE_BATTERY_4 23
#define TEXTURE_BATTERY_5 24
#define TEXTURE_WIFI_DISCONNECTED 25
#define TEXTURE_WIFI_0 26
#define TEXTURE_WIFI_1 27
#define TEXTURE_WIFI_2 28
#define TEXTURE_WIFI_3 29
#define TEXTURE_AUTO_START 30

#define NUM_COLORS 6

#define COLOR_TEXT 0
#define COLOR_NAND 1
#define COLOR_SD 2
#define COLOR_GAME_CARD 3
#define COLOR_DS_TITLE 4
#define COLOR_DIRECTORY 5

void screen_init();
void screen_exit();
void screen_load_texture(u32 id, void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
u32 screen_load_texture_auto(void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
void screen_load_texture_file(u32 id, const char* path, bool linearFilter);
u32 screen_load_texture_file_auto(const char* path, bool linearFilter);
void screen_load_texture_tiled(u32 id, void* tiledData, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
u32 screen_load_texture_tiled_auto(void* tiledData, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
void screen_unload_texture(u32 id);
void screen_get_texture_size(u32* width, u32* height, u32 id);
void screen_begin_frame();
void screen_end_frame();
void screen_select(gfxScreen_t screen);
void screen_draw_texture(u32 id, float x, float y, float width, float height);
void screen_draw_texture_crop(u32 id, float x, float y, float width, float height);
void screen_get_string_size(float* width, float* height, const char* text, float scaleX, float scaleY);
void screen_draw_string(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool baseline);