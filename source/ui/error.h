#pragma once

typedef struct ui_view_s ui_view;

void error_display(volatile bool* dismissed, void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), const char* text, ...);
void error_display_res(volatile bool* dismissed, void* data, void (* drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...);
void error_display_errno(volatile bool* dismissed, void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...);