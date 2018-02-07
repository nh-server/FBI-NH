#pragma once

typedef struct __sFILE FILE;

#define TOP_SCREEN_WIDTH 400
#define TOP_SCREEN_HEIGHT 240

#define BOTTOM_SCREEN_WIDTH 320
#define BOTTOM_SCREEN_HEIGHT 240

#define MAX_TEXTURES 1024
#define MAX_COLORS 32

#define COLOR_TEXT 0

void screen_init();
void screen_exit();
void screen_set_base_alpha(u8 alpha);
void screen_set_color(u32 id, u32 color);
u32 screen_allocate_free_texture();
void screen_load_texture_untiled(u32 id, void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
void screen_load_texture_path(u32 id, const char* path, bool linearFilter);
void screen_load_texture_file(u32 id, FILE* fd, bool linearFilter);
void screen_load_texture_tiled(u32 id, void* data, u32 size, u32 width, u32 height, GPU_TEXCOLOR format, bool linearFilter);
void screen_unload_texture(u32 id);
void screen_get_texture_size(u32* width, u32* height, u32 id);
void screen_begin_frame();
void screen_end_frame();
void screen_select(gfxScreen_t screen);
void screen_draw_texture(u32 id, float x, float y, float width, float height);
void screen_draw_texture_crop(u32 id, float x, float y, float width, float height);
float screen_get_font_height(float scaleY);
void screen_get_string_size(float* width, float* height, const char* text, float scaleX, float scaleY);
void screen_get_string_size_wrap(float* width, float* height, const char* text, float scaleX, float scaleY, float wrapWidth);
void screen_draw_string(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool centerLines);
void screen_draw_string_wrap(const char* text, float x, float y, float scaleX, float scaleY, u32 colorId, bool centerLines, float wrapX);