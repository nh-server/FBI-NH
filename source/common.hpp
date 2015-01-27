#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <vector>
#include <string>
#include <functional>

#define printf platform_printf

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef struct {
    u8 r;
    u8 g;
    u8 b;
} Color;

typedef enum {
    TOP_SCREEN,
    BOTTOM_SCREEN
} Screen;

typedef enum {
    SELECTED,
    BACK,
    APP_CLOSING,
    MANUAL_BREAK
} SelectionResult;

typedef enum {
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

typedef struct {
    int x;
    int y;
} Touch;

typedef enum {
    NAND,
    SD
} MediaType;

typedef enum {
    WII,
    DSI,
    THREEDS,
    WIIU,
    UNKNOWN_PLATFORM
} AppPlatform;

// TODO: verify categories.
typedef enum {
    APP,
    DLC,
    PATCH,
    SYSTEM,
    TWL
} AppCategory;

typedef struct {
    u64 titleId;
    u32 uniqueId;
    char productCode[16];
    MediaType mediaType;
    AppPlatform platform;
    AppCategory category;
} App;

bool screen_begin_draw(Screen screen);
bool screen_end_draw();
void screen_swap_buffers_quick();
void screen_swap_buffers();
void screen_take_screenshot();
int screen_get_width();
int screen_get_height();
void screen_draw(int x, int y, u8 r, u8 g, u8 b);
void screen_fill(int x, int y, int width, int height, u8 r, u8 g, u8 b);
int screen_get_str_width(std::string str);
int screen_get_str_height(std::string str);
void screen_draw_string(std::string str, int x, int y, u8 r, u8 g, u8 b);
void screen_clear(u8 r, u8 g, u8 b);

bool ui_select_file(std::string rootDirectory, std::string extension, std::string* selectedFile, std::function<bool()> onLoop);
bool ui_select_app(MediaType mediaType, App* selectedApp, std::function<bool()> onLoop);

void input_poll();
bool input_is_released(Button button);
bool input_is_pressed(Button button);
bool input_is_held(Button button);
Touch input_get_touch();

std::string app_get_platform_name(AppPlatform platform);
std::string app_get_category_name(AppCategory category);
std::vector<App> app_list(MediaType mediaType);
bool app_install(MediaType mediaType, std::string path, std::function<bool(int)> onProgress);
bool app_delete(App app);
bool app_launch(App app);

u64 fs_get_free_space(MediaType mediaType);

bool platform_init();
void platform_cleanup();
bool platform_is_running();
u64 platform_get_time();
void platform_delay(int ms);
void platform_printf(const char* format, ...);

#endif