#pragma once

#define R_FBI_CANCELLED MAKERESULT(RL_PERMANENT, RS_CANCELED, RM_APPLICATION, 1)
#define R_FBI_ERRNO MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 2)
#define R_FBI_HTTP_RESPONSE_CODE MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 3)
#define R_FBI_WRONG_SYSTEM MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, 4)
#define R_FBI_INVALID_ARGUMENT MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, 5)
#define R_FBI_THREAD_CREATE_FAILED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 6)
#define R_FBI_PARSE_FAILED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 7)
#define R_FBI_BAD_DATA MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 8)

#define R_FBI_NOT_IMPLEMENTED MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, RD_NOT_IMPLEMENTED)
#define R_FBI_OUT_OF_MEMORY MAKERESULT(RL_FATAL, RS_OUTOFRESOURCE, RM_APPLICATION, RD_OUT_OF_MEMORY)
#define R_FBI_OUT_OF_RANGE MAKERESULT(RL_PERMANENT, RS_INVALIDARG, RM_APPLICATION, RD_OUT_OF_RANGE)

typedef struct ui_view_s ui_view;

ui_view* error_display(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), const char* text, ...);
ui_view* error_display_res(void* data, void (* drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), Result result, const char* text, ...);
ui_view* error_display_errno(void* data, void (*drawTop)(ui_view* view, void* data, float x1, float y1, float x2, float y2), int err, const char* text, ...);