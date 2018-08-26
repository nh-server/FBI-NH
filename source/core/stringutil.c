#include <string.h>

#include <3ds.h>

#include "stringutil.h"

bool string_is_empty(const char* str) {
    if(strlen(str) == 0) {
        return true;
    }

    const char* curr = str;
    while(*curr) {
        if(*curr != ' ') {
            return false;
        }

        curr++;
    }

    return true;
}

void string_copy(char* dst, const char* src, size_t size) {
    if(size > 0) {
        strncpy(dst, src, size - 1);
        dst[size - 1] = '\0';
    }
}

void string_get_file_name(char* out, const char* file, u32 size) {
    const char* end = file + strlen(file);
    const char* curr = file - 1;
    while((curr = strchr(curr + 1, '.')) != NULL) {
        end = curr;
    }

    u32 terminatorPos = end - file < size - 1 ? end - file : size - 1;
    strncpy(out, file, terminatorPos);
    out[terminatorPos] = '\0';
}

void string_escape_file_name(char* out, const char* file, size_t size) {
    static const char reservedChars[] = {'<', '>', ':', '"', '/', '\\', '|', '?', '*'};

    for(u32 i = 0; i < size; i++) {
        bool reserved = false;
        for(u32 j = 0; j < sizeof(reservedChars); j++) {
            if(file[i] == reservedChars[j]) {
                reserved = true;
                break;
            }
        }

        if(reserved) {
            out[i] = '_';
        } else {
            out[i] = file[i];
        }

        if(file[i] == '\0') {
            break;
        }
    }
}

void string_get_path_file(char* out, const char* path, u32 size) {
    const char* start = NULL;
    const char* end = NULL;
    const char* curr = path - 1;
    while((curr = strchr(curr + 1, '/')) != NULL) {
        start = end != NULL ? end : path;
        end = curr;
    }

    if(end != path + strlen(path) - 1) {
        start = end;
        end = path + strlen(path);
    }

    if(end - start == 0) {
        strncpy(out, "/", size);
    } else {
        u32 terminatorPos = end - start - 1 < size - 1 ? end - start - 1 : size - 1;
        strncpy(out, start + 1, terminatorPos);
        out[terminatorPos] = '\0';
    }
}

void string_get_parent_path(char* out, const char* path, u32 size) {
    size_t pathLen = strlen(path);

    const char* start = NULL;
    const char* end = NULL;
    const char* curr = path - 1;
    while((curr = strchr(curr + 1, '/')) != NULL && (start == NULL || curr != path + pathLen - 1)) {
        start = end != NULL ? end : path;
        end = curr;
    }

    u32 terminatorPos = end - path + 1 < size - 1 ? end - path + 1 : size - 1;
    strncpy(out, path, terminatorPos);
    out[terminatorPos] = '\0';
}