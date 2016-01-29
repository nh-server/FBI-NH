#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <3ds.h>

#include "error.h"
#include "prompt.h"

typedef struct {
    char fullText[4096];
    void* data;
    void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2);
} error_data;

static void error_draw_top(ui_view* view, void* data, float x1, float y1, float x2, float y2) {
    error_data* errorData = (error_data*) data;

    if(errorData->drawTop != NULL) {
        errorData->drawTop(view, errorData->data, x1, y1, x2, y2);
    }
}

static void error_onresponse(ui_view* view, void* data, bool response) {
    prompt_destroy(view);
    free(data);
}

void error_display_res(void* data, void (* drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    // TODO: Break result codes down into their parts.
    snprintf(errorData->fullText, 4096, "%s\nResult code: 0x%08lX", textBuf, result);

    ui_push(prompt_create("Error", errorData->fullText, 0xFF000000, false, errorData, NULL, error_draw_top, error_onresponse));
}

void error_display_errno(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...) {
    error_data* errorData = (error_data*) calloc(1, sizeof(error_data));
    errorData->data = data;
    errorData->drawTop = drawTop;

    char textBuf[1024];
    va_list list;
    va_start(list, text);
    vsnprintf(textBuf, 1024, text, list);
    va_end(list);

    snprintf(errorData->fullText, 4096, "%s\nError: %s (%d)", textBuf, strerror(err), err);

    ui_push(prompt_create("Error", errorData->fullText, 0xFF000000, false, errorData, NULL, error_draw_top, error_onresponse));
}