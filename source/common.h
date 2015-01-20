#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#define printf platform_printf

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef struct _color {
    u8 r;
    u8 g;
    u8 b;
} Color;

typedef enum _button {
    BUTTON_A,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y,
    BUTTON_L,
    BUTTON_R,
    BUTTON_START,
    BUTTON_SELECT,
    BUTTON_UP,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_TOUCH
} Button;

typedef struct _touch {
    int x;
    int y;
} Touch;

typedef enum _media_type {
    NAND,
    SD
} MediaType;

typedef enum _app_platform {
    WII,
    DSI,
    THREEDS,
    WIIU,
    UNKNOWN_PLATFORM
} AppPlatform;

// TODO: verify categories.
typedef enum _app_category {
    APP,
    DLC,
    PATCH,
    SYSTEM,
    TWL
} AppCategory;

typedef struct _app {
    u64 titleId;
    u32 uniqueId;
    char productCode[16];
    MediaType mediaType;
    AppPlatform platform;
    AppCategory category;
} App;

char* sdprintf(const char* format, ...);
char* vsdprintf(const char* format, va_list args);

bool screen_begin_draw();
bool screen_begin_draw_info();
bool screen_end_draw();
void screen_swap_buffers_quick();
void screen_swap_buffers();
void screen_take_screenshot();
int screen_get_width();
int screen_get_height();
void screen_draw(int x, int y, u8 r, u8 g, u8 b);
void screen_fill(int x, int y, int width, int height, u8 r, u8 g, u8 b);
int screen_get_str_width(const char* str);
int screen_get_str_height(const char* str);
void screen_draw_string(const char* string, int x, int y, u8 r, u8 g, u8 b);
void screen_clear(u8 r, u8 g, u8 b);
void screen_clear_all();

void input_poll();
bool input_is_released(Button button);
bool input_is_pressed(Button button);
bool input_is_held(Button button);
Touch input_get_touch();

const char* app_get_platform_name(AppPlatform platform);
const char* app_get_category_name(AppCategory category);
App* app_list(MediaType mediaType, u32* count);
bool app_install(MediaType mediaType, const char* path, bool (*onProgress)(int progress));
bool app_delete(MediaType mediaType, App app);
bool app_launch(MediaType mediaType, App app);

void platform_init();
void platform_cleanup();
bool platform_is_running();
u64 platform_get_time();
void platform_delay(int ms);
void platform_print(const char* str);
void platform_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif