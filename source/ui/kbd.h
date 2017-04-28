#pragma once

typedef struct ui_view_s ui_view;

ui_view* kbd_display(const char* hint, const char* initialText, SwkbdType type, u32 features, SwkbdValidInput validation, u32 maxSize, void* data, void (*onResponse)(ui_view* view, void* data, SwkbdButton button, const char* response));